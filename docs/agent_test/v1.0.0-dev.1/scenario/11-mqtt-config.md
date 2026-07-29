# 11 — MQTT Dynamic Config

Precondition: `00-setup.md` complete. `DEVICE_ID`, `MQTT_HOST`, `MQTT_PORT`,
`MQTT_USER`, `MQTT_PASS` from `00-setup.md` available. This scenario was
verified using `paho-mqtt` (Python) directly against the real broker rather
than `mosquitto_pub`, since the payload needed no extra tooling.

## Topic / payload shape

`/sub/{device_id}/config` — JSON `{"key": "<preloaded_schema_key>", "value":
"<string>"}` (`value` is always sent as a string; the handler parses/casts
per-key as needed). Handled by `pres_mqtt_handler_config` (`main/src/
presentation/mqtt/handler/config/handler.c`), which:
1. Rejects malformed JSON or a missing `key`/`value` field
   (`"Invalid config payload: missing key or value field"`, logged locally,
   no ack/response published on this topic at all — matches the
   action/registration handlers' precedent of silent-drop-with-local-log for
   malformed input).
2. Looks up `key` against `dom_models_preloaded_schema` (the same 7-entry
   schema exposed read-only over BLE's System Info `config_schema`
   characteristic, see `10-ble-gatt-services.md`'s BLE-12). Unknown key →
   `"Unknown config key: %s"`, dropped.
3. For a known key, the *type* interpretation comes from the matched schema
   entry's declared `type` (STRING/UINT32/BOOL), and the key name only
   selects which update field to populate. STRING keys (`mqtt_proto`/
   `mqtt_host`/`mqtt_port`/`mqtt_user`/`mqtt_pass`) are copied straight
   through. UINT32 (`sys_rst_aft_ms`) is parsed with `strtoul`; an
   invalid/partial parse, an empty string, **or a value of `0`** →
   `"Invalid config value for %s: %s"`, dropped. `0` is rejected explicitly
   because the boot-time `settings->restart(...)` call in `app_main` has no
   zero-delay guard, so persisting `0` would restart the device immediately
   on every boot — an unrecoverable boot loop. BOOL (`wifi_try_init`) only
   accepts the literal strings `"true"`/`"false"` (anything else → same
   invalid-value log), but even a valid value is then rejected with
   `"Config key %s is not writable via MQTT (MQTT context has no
   wifi_manager reference; use BLE)"`. Note this is a *wiring* gap, not a
   missing capability: `dom_usecases_internal_wifi_manager_t
   .set_try_connect_on_init` exists and BLE's wifi_manager handler already
   writes this setting successfully (see BLE-08 in
   `10-ble-gatt-services.md`). The MQTT handler simply has no
   `wifi_manager` reference on `pres_mqtt_context_t` to call it through, and
   `dom_usecases_internal_settings_preloaded_update_t` — the only path it
   does hold — carries no field for this key.
   A schema key with no branch at all falls into an explicit terminal error
   (`"Config key %s has no MQTT handler mapping despite being in the
   schema"`) rather than a silent empty update.
4. A successful update goes through `ctx->settings->set_preloaded`, logging
   `"Config value updated successfully via MQTT: %s"` on success (failure is
   logged once, inside the usecase itself, to avoid double-logging).

## Setup for this pass

Device: ESP32-C3, `DEVICE_ID=AC276E5E030C`, freshly flashed from `development`
at the tip containing this feature. Boot log confirmed a clean boot, WiFi
connect, MQTT connect, registration, and:
```
[INFO] [internal_messaging_callbacks/subscribe_defaults] Default subscriptions completed successfully
```
— confirms the `config` topic subscription itself succeeded (no `"Failed to
subscribe to config"` error logged).

Verification script: published each payload below to
`/sub/AC276E5E030C/config` over TLS (qos 1) using `paho-mqtt`, with a
concurrent serial capture (`/dev/ttyACM0`, 115200) to observe the resulting
log lines in real time.

## Results — this pass

- [x] **CFG-01** — `{"key":"mqtt_proto","value":"mqtts"}` (positive):
  device logs `Received config request via MQTT` →
  `Preloaded settings updated successfully` →
  `Config value updated successfully via MQTT: mqtt_proto`. Confirms the
  settings usecase's `set_preloaded` path completed successfully end to end
  for a known string key. (The value written, `mqtts`, matched the device's
  already-active protocol, so no visible reconnect/behavior change was
  expected or observed beyond the success log — the write path itself is
  what's under test here.)
- [x] **CFG-02** — `{"key":"nonexistent_key","value":"x"}` (negative):
  device logs `Unknown config key: nonexistent_key` and continues running
  normally (WiFi/MQTT reconnect-check logs kept ticking on schedule
  afterward) — no crash.
- [x] **CFG-03** — `{"key":"sys_rst_aft_ms","value":"not_a_number"}`
  (negative): device logs
  `Invalid config value for sys_rst_aft_ms: not_a_number` and continues
  running normally — no crash.
- [x] **CFG-04** — `{"key":"wifi_try_init","value":"true"}` (negative,
  documents the not-writable-via-MQTT limitation): device logs the
  not-writable-via-MQTT warning for `wifi_try_init` and continues running
  normally — no crash. Note this is a *valid* boolean value (unlike CFG-03)
  that still gets rejected, specifically because the MQTT context holds no
  `wifi_manager` reference (the setting itself is writable over BLE), not
  because of a parse failure. See `09-known-gaps-summary.md`.

The first four cases matched the handler's documented behavior exactly, and
the device never crashed or lost its MQTT connection across the run.

## Results — boot-loop guard pass (CFG-05)

Added after a whole-branch review found that `sys_rst_aft_ms` = `"0"` was
accepted and persisted, which would restart the device with no delay on
every subsequent boot — an unrecoverable boot loop, reachable from a
well-formed MQTT config push. Fixed by rejecting `0` (and the empty string)
in the handler's UINT32 branch.

- [x] **CFG-05** — `{"key":"sys_rst_aft_ms","value":"0"}` (negative,
  boot-loop guard): device logs
  `Invalid config value for sys_rst_aft_ms: 0`, does **not** restart, does
  **not** persist the value, and keeps running normally (WiFi/MQTT
  reconnect-check logs kept ticking afterward). Re-verified on hardware
  after the fix.

Not exercised this pass: `mqtt_host`/`mqtt_port`/`mqtt_user`/`mqtt_pass`
writes (same string-copy path as `mqtt_proto`, CFG-01, so lower marginal
value) and a value change for `mqtt_proto` that would actually flip the live
connection (e.g. `mqtts` → a value that changes transport), to avoid
disrupting the shared broker connection mid-pass.
