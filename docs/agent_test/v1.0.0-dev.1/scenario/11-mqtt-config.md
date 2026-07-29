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
3. For a known key: `mqtt_proto`/`mqtt_host`/`mqtt_port`/`mqtt_user`/
   `mqtt_pass` are copied straight through as strings;
   `sys_rst_aft_ms` is parsed with `strtoul` (invalid/partial parse →
   `"Invalid config value for %s: %s"`, dropped); `wifi_try_init` only
   accepts the literal strings `"true"`/`"false"` (anything else → same
   invalid-value log) but even a valid value is then rejected as read-only
   (`"Config key %s is read-only via MQTT (not yet supported by the
   settings usecase)"`) because the settings usecase has no update path for
   it yet — it exists in the schema purely for BLE/read visibility.
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
  documents read-only-via-MQTT limitation): device logs
  `Config key wifi_try_init is read-only via MQTT (not yet supported by the
  settings usecase)` and continues running normally — no crash. Note this
  is a *valid* boolean value (unlike CFG-03) that still gets rejected,
  specifically because the settings usecase has no update path for this key
  yet, not because of a parse failure.

No bugs found this pass — all four cases matched the handler's documented
behavior exactly, and the device never crashed or lost its MQTT connection
across the run.

Not exercised this pass: `mqtt_host`/`mqtt_port`/`mqtt_user`/`mqtt_pass`
writes (same string-copy path as `mqtt_proto`, CFG-01, so lower marginal
value) and a value change for `mqtt_proto` that would actually flip the live
connection (e.g. `mqtts` → a value that changes transport), to avoid
disrupting the shared broker connection mid-pass.
