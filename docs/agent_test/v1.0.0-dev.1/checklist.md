# Full Feature Test Checklist — v1.0.0-dev.1

Hardware-in-the-loop checklist: the real ESP32-C3 device (`/dev/ttyACM0`)
running this firmware, talking to the real `mate-things` backend over a real
WiFi AP and the real HiveMQ Cloud MQTT broker. Nothing here is mocked.

Run scenarios in order (00 is setup, not a test section). Check each box as
it's executed and passes as documented; leave unchecked + annotate if it
deviates from the documented expectation (cross-reference
`09-known-gaps-summary.md` for anything already flagged as a known gap so a
deviation there isn't mistaken for a new bug).

**BLE was out of scope for the original 42 cases below** — this dev machine
had no Bluetooth adapter at the time, so WiFi/MQTT/OTA cases were driven by
NVS values written directly (via the `test_seed` composition — see
`00-setup.md`) instead of BLE provisioning. A Bluetooth adapter has since
become available; the BLE GATT surface (settings, wifi_manager, log
services) was tested separately as `scenario/10-ble-gatt-services.md` —
see `09-known-gaps-summary.md` for the three real crash/discoverability
bugs found and fixed there.

Total test cases: 42 (+ 11 BLE cases in scenario 10, not counted here)

Before running anything, complete [`00-setup.md`](scenario/00-setup.md)
(seed NVS, flash the real firmware, get a backend access token, upload a
firmware binary).

## 01 — Boot & NVS Preloaded Config (`scenario/01-boot-and-config.md`)

- [x] **BOOT-01** — Cold boot with blank NVS: sane compiled-in defaults, no crash (positive)
- [x] **BOOT-02** — Boot after `test_seed`: WiFi/MQTT credentials read back correctly (positive)
- [x] **BOOT-03** — `idf.py flash` (normal firmware) does not erase the NVS partition (positive, mechanism check)
- [x] **BOOT-04** — Device ID is derived from MAC and stable across reboots (positive — `AC276E5E030C` stable across every reboot/reflash this session)
- [x] **BOOT-05** — `system_restart_after_ms` default sentinel does not cause an unwanted restart loop (edge — no unexpected restarts observed across the whole session; value confirmed in code as `0xFFFFFFFF`)

## 02 — WiFi Connection (`scenario/02-wifi-connection.md`)

- [x] **WIFI-01** — STA connects to the seeded AP within a few seconds of boot (positive)
- [x] **WIFI-02** — `internal_wifi_manager` logs "WiFi connected, IP: ..." exactly once per connect (positive, new session feature)
- [ ] **WIFI-03** — "WiFi disconnected" logged when the AP is turned off / out of range (positive, new session feature) — **skipped**: requires physically power-cycling the shared home AP or moving the device out of range, neither possible remotely in this environment
- [x] **WIFI-04** — Wrong WiFi password: connection fails, retried, no crash (negative)
- [x] **WIFI-05** — `wifi_sta_try_connect_on_init=false` (unseeded default): device does not attempt STA connect on boot (negative/edge — confirmed via the same blank-NVS capture as BOOT-01)

## 03 — MQTT Registration & Status (`scenario/03-mqtt-registration-status.md`)

- [x] **REG-01** — Device publishes `/pub/registration` with correct `device_id`/`device_info`/`firmware_name` on MQTT connect (positive)
- [x] **REG-02** — Backend accepts it and creates/finds a node row (positive, ⚠ known gap — see 09, firmware_name charset/separator)
- [x] **REG-03** — Device receives `/sub/<device_id>/registration_ack` and logs it, no side effect (positive, confirms no-op)
- [x] **REG-04** — Registration when no matching `firmware_name` row exists on the backend: silently dropped, no ack, node not created (negative, matches backend's own MQTT-01 note)
- [x] **STAT-01** — `/pub/<device_id>/status` `{"status":"ONLINE"}` published on connect, backend reflects `is_connected: true` (positive)
- [ ] **STAT-02** — LWT publishes `{"status":"OFFLINE"}` (retained) when the device drops off ungracefully (positive) — **skipped**: requires abruptly unplugging the device's USB power, not performed this pass to avoid disrupting the ongoing test session

## 04 — Logging Observability (`scenario/04-logging.md`)

- [x] **LOG-01** — Device log lines are published to `/pub/<device_id>/log` and observable via `mosquitto_sub` (positive)
- [x] **LOG-02** — Log messages continue during/after a WiFi reconnect cycle (positive; approximated with a full device reset rather than an AP power-cycle — AP is not remotely controllable — but confirms the log→MQTT path isn't stuck/deadlocked across a connection cycle)

## 05 — Action Dispatch (`scenario/05-action-dispatch.md`)

- [x] **ACT-01** — Backend dispatches the seeded `restart` action, device receives `/sub/<device_id>/action` (positive)
- [x] **ACT-02** — Device immediately ACKs `SUCCESS` before actually restarting (positive)
- [x] **ACT-03** — Device restarts after `payload.delay_ms`, re-registers afterward (positive)
- [x] **ACT-04** — `payload.delay_ms` omitted: defaults to 0 (edge)
- [x] **ACT-05** — Action message missing `execution_id`: silently dropped, no ack (negative)
- [x] **ACT-06** — Action message missing `action` field: `FAILED` ack with "missing action field" (negative)
- [x] **ACT-07** — Unknown action name (anything other than `restart`): `FAILED` ack "unknown action" (negative, documents the single-action limitation)
- [x] **ACT-08** — Backend action log reflects the device's ack status correctly (positive, cross-check)

## 06 — OTA Update (`scenario/06-ota-update.md`)

- [x] **OTA-01** — `upload.py` uploads/replaces the firmware binary and returns a valid presigned URL (positive, tooling check)
- [x] **OTA-02** — Backend OTA dispatch reaches the device on `/sub/<device_id>/ota` with `firmware_url`/`firmware_size`/`firmware_checksum` (positive; ⚠ known gap found and fixed — see 09, firmware_url buffer truncation)
- [x] **OTA-03** — Device downloads and flashes the new image via `esp_https_ota`, reboots into it (positive)
- [x] **OTA-04** — Post-OTA registration reflects the (unchanged, since same source) firmware name/version correctly (positive, sanity check)
- [x] **OTA-05** — OTA with a checksum mismatch is rejected, device does not boot into corrupt firmware (negative)
- [x] **OTA-06** — OTA with a URL that 404s / is unreachable: fails gracefully, no crash, no ack topic exists for OTA so failure is local-only (negative)
- [ ] **OTA-07** — OTA rollback: a build that fails `ota->validate()` on first boot after update rolls back to the previous partition (negative, ⚠ requires deliberately broken build, may be skipped if too destructive/time-consuming — annotate if skipped) — **skipped**: time/risk budget for this pass; would require a deliberately broken build and careful recovery

## 07 — Settings Persistence (`scenario/07-settings-persistence.md`)

- [x] **SET-01** — MQTT config read from NVS on boot matches what `test_seed` wrote (positive)
- [x] **SET-02** — Values survive a normal `idf.py flash` + reboot cycle (positive, confirms BOOT-03's mechanism from the settings' perspective)
- [x] **SET-03** — Values survive an OTA update (positive, confirms NVS partition is untouched by OTA app-partition writes too)

## 08 — Resilience & Reconnect (`scenario/08-resilience.md`)

- [ ] **RESIL-01** — MQTT broker connection drop and automatic reconnect (positive) — **skipped**: requires toggling the shared AP's WiFi, not remotely controllable
- [ ] **RESIL-02** — Per-device topic subscriptions survive an MQTT reconnect (positive) — **skipped**: depends on RESIL-01
- [x] **RESIL-03** — Backend container restart mid-session: device reconnects and re-registers once backend is back (positive; ⚠ known gap found and fixed — see 09, `is_connected` staleness from a non-retained status publish)
- [ ] **RESIL-04** — WiFi AP restart: device's `wifi_sta_reconnect` watchdog task reconnects without a manual power cycle (positive) — **skipped**: requires physically power-cycling the shared home AP

## 10 — BLE GATT Services (`scenario/10-ble-gatt-services.md`)

Added once a Bluetooth adapter became available; not part of the original
42 cases. See the scenario file for full detail and the protocol
reference.

- [x] **BLE-01** — Device advertises and is discoverable by name (positive; ⚠ known gap found and fixed — see 09, advertising-payload overflow)
- [x] **BLE-02** — GATT discovery matches the documented UUID/property table (positive)
- [x] **BLE-03** — Settings `data` read returns a valid snapshot (positive; ⚠ known gap found and fixed — see 09, stack-overflow crash)
- [x] **BLE-04** — Settings `update` write + `restart_required` reflects the change (positive)
- [x] **BLE-05** — Settings `data` re-read shows the persisted change (positive)
- [x] **BLE-06** — WiFi manager `status` read reflects live connection state (positive)
- [x] **BLE-07** — WiFi manager `stored_credential` read reflects the seeded SSID (positive)
- [x] **BLE-08** — WiFi manager `try_connect_on_init` read returns the seeded flag (positive)
- [x] **BLE-09** — Log `enabled` read/write round-trips correctly (positive)
- [x] **BLE-10** — WiFi manager `command` write (disconnect/connect_stored) triggers reconnect with zero crashes (positive; ⚠ known gap found and fixed — see 09, second stack-overflow crash)
- [ ] **BLE-11** — Log message notifications delivered during a WiFi reconnect — **not confirmed**: 0 notifications observed, likely BLE/WiFi radio coexistence, not investigated further

## Reference

See [`09-known-gaps-summary.md`](scenario/09-known-gaps-summary.md) for the
consolidated results after running this checklist — written *after*
execution, not predicted in advance.
