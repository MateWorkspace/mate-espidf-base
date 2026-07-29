# 09 — Known Gaps Summary — Results

Written after running the checklist against the real device + real backend,
not predicted in advance. Updated as execution proceeds.

## 0 — Critical bugs found during setup / early scenario execution

| # | Area | Bug | Status |
|---|---|---|---|
| 1 | Registration / firmware-name interop | Device registration payload sends `firmware_name = "<PROJECT_NAME>_<PROJECT_VERSION>"` (`app_internal_messaging_callbacks_impl_build_firmware_name`), which for this project is `"mate-espidf-base_v1.0.0-dev.1"` — contains dots from the version string. The backend's `RequiredFirmwareName` validation (`mate-things/backend/internal/application/shared/validation.go`) only allowed `[A-Za-z0-9_-]`, no dots, so a firmware row with that exact name could never be created, and `UpsertRegistration`'s DB join requires an exact match — **registration would silently fail forever** (no error surfaced to the device, no ack, nothing in backend logs beyond a repo-level not-found). Confirmed via `docker exec`-free reasoning + `upload.py` reproducing the `400` directly. | **Fixed** — loosened `firmwareNamePattern` to `[A-Za-z0-9_.-]`, rebuilt+redeployed the backend container. Verified: `upload.py` now successfully creates the firmware row with the exact device-sent name, confirmed via `GET /v1/firmwares/by-name/...` returning `200`. |
| 2 | Build / firmware version compile-time macro | `main/CMakeLists.txt` defined `PROJECT_VER` from a custom `PROJECT_VERSION` CMake variable set in the root `CMakeLists.txt`, but CMake's `project()` command always redefines `PROJECT_VERSION` itself as part of processing components (ESP-IDF's component processing happens *inside* `project()`), silently clobbering the custom value to empty regardless of set order/CACHE/FORCE. This left the compile-time `PROJECT_VER` macro empty/`"unknown"` (via `esp_impl_types.h`'s `#ifndef` fallback) even though the runtime `esp_app_desc_t`/boot-banner version (populated separately, via ESP-IDF's own `version.txt` mechanism) looked correct — two independent version-propagation mechanisms, only one of which was fixed by adding `version.txt`. This broke the device's own registration payload (`firmware_name` ending in `_unknown`, matching no real firmware row). | **Fixed** — added `version.txt` at repo root (runtime version) and `idf_build_get_property(mate_project_ver PROJECT_VER)` + `target_compile_definitions(... PROJECT_VER="${mate_project_ver}")` in `main/CMakeLists.txt` (compile-time macro, reading back ESP-IDF's own already-resolved value instead of a custom variable). Removed the dead custom `PROJECT_VERSION` plumbing. Verified: boot banner shows `App version: v1.0.0-dev.1`, backend registration succeeds with the correct firmware name. |
| 3 | OTA / firmware URL truncation | `dom_models_update_info_t.firmware_url` (`main/include/domain/models/update.h`) was a fixed `char[256]` buffer. Real MinIO presigned download URLs (AWS SigV4 query string: algorithm, credential, date, expiry, signed-headers, content-disposition, signature) run 400+ characters — well past 256. The MQTT OTA handler's `strncpy` into this buffer silently truncated the URL mid-query-string (confirmed: device log showed the URL cut off at exactly 255 characters, mid-`X-Amz-SignedHeaders` param), producing a URL with an incomplete/invalid signature. MinIO correctly rejected it (`403 SignatureDoesNotMatch`), which surfaced on-device only as a generic `OTA update failed: FAILURE (8)` with no indication the URL itself was mangled — every real (non-toy) OTA dispatch would have failed this way. Confirmed by measuring the actual presigned URL length (423 chars) against the 256-byte buffer, and reproducing the exact truncation point. | **Fixed** — bumped `DOM_MODELS_UPDATE_URL_MAX_LEN` from 256 to 512 and made the struct field use the macro instead of a separately hardcoded `256` (was previously defined-but-unused, a latent inconsistency). Verified end-to-end: full OTA dispatch cycle (download progress 0%→100%, checksum match, partition switch, restart, WiFi reconnect, re-registration) completed successfully against the real backend + real presigned URL. |
| 4 | Backend resilience / `is_connected` staleness (RESIL-03) | `docker restart mate-things-app-1` mid-session, then checked `is_connected` immediately and 8s/16s later: stayed `false` the whole time even though the device's own MQTT session to HiveMQ Cloud was never interrupted by the backend restart (the device only talks to the broker, not to the backend process, for the transport layer). Root cause: the device's `/pub/<device_id>/status` publish (`inf_messaging_def_pub_mqtt_impl`) was sent with `retain=0` and only fires on the *device's own* MQTT connect event — a backend-only restart gave the device no reason to re-publish. The backend's `Resubscribe()` (`messaging_callback/usecase.go`) does correctly re-subscribe to every known device's status topic on reconnect (confirmed by reading the code), but a fresh subscription to a non-retained topic receives nothing until a new message is actually published. The asymmetry: the device's own LWT (`{"status":"OFFLINE"}`, configured in `driver.c`) was already retained on the exact same topic — only the explicit `ONLINE` publish wasn't, an inconsistency within the firmware itself, not something requiring a backend change. | **Fixed** — added a `retain` parameter to `inf_messaging_def_pub_mqtt_impl_publish_json` (`main/include/...def_pub/mqtt_impl_utils.h` / `.c`), set to `true` only for the status publish in `mqtt_impl.c` (registration and action_ack stay unretained — they're events, not state). Now `ONLINE`/`OFFLINE` retained messages on `/pub/<device_id>/status` correctly overwrite each other regardless of which one (explicit publish vs. LWT) fired last. Verified: restarted the backend container with the device continuously connected and never reset — `is_connected` stayed `true` throughout, immediately correct on the backend's very first re-subscribe, with zero device-side intervention (matching the checklist's original expectation exactly). |
| 5 | BLE advertising never starts | `[ble/host/start_advertising] Failed to set advertising fields: 4` (NimBLE `BLE_HS_EMSGSIZE`) on every boot. Root cause: `cmp_main_utils_build_ble_device_name` formatted `"<PROJECT_NAME>-<device_id_str>"` = `"mate-espidf-base-AC276E5E030C"` (29 chars). The device-name AD field (2 + 29 = 31 bytes) plus the flags AD field (3 bytes) totals 34 bytes, over the legacy advertising PDU's 31-byte limit, so `ble_gap_adv_set_fields()` always failed and the device was never discoverable/connectable over BLE at all. Confirmed by computing the AD field sizes and reproducing on real hardware with a real Bluetooth adapter (`bluetoothctl`/`bleak`) for the first time this pass. | **Fixed** — `cmp_main_utils_build_ble_device_name` (`main/src/composition/main/utils.c`) now formats `"matedev_<device_id_str>"` (20 chars, well under the limit). Verified: boot log now shows `[ble/host/start_advertising] BLE advertising started`, and the device is discoverable via `bluetoothctl scan on` / `bleak` as `matedev_AC276E5E030C`. |
| 6 | BLE settings/wifi_manager GATT crash (stack overflow) | Reading the settings `data` characteristic (and, by the same pattern, any settings/wifi_manager characteristic using a stack-local JSON/payload buffer) crashed the device: `Guru Meditation Error: Core 0 panic'ed (Stack protection fault)` in task `nimble_host` immediately after `[internal_settings/get_snapshot] Settings snapshot retrieved successfully` logged. Root cause: `settings/handler.c`'s `data_access_callback` (and the update/restart_required callbacks, and wifi_manager's status/connect/stored_credential callbacks) declared their JSON/payload buffers (96–512 bytes) as **stack-local** variables inside a GATT access callback that runs on the `nimble_host` FreeRTOS task, whose configured stack (`CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=4096`) is already substantially used by NimBLE's own ATT/GATT call chain by the time the callback runs — unlike the log handler, which already used struct-owned (not stack-local) storage for the same reason. | **Fixed** — moved all these buffers into the handler structs (`pres_ble_handler_settings_t`/`pres_ble_handler_wifi_manager_t` in their respective `types.h`) as struct-owned fields, matching the log handler's existing safe pattern; each handler has exactly one instance, so no synchronization is needed. Verified: full read/write cycle against every settings and wifi_manager characteristic over real BLE with zero crashes, including a settings-data write immediately followed by a snapshot read confirming the change. |
| 7 | BLE log-forwarding GATT crash (stack overflow, different task) | With BLE log forwarding enabled (write `1` to the log service's `enabled` characteristic) and a central subscribed, triggering a WiFi disconnect (which cascades into MQTT transport-error logging) crashed the device: `Guru Meditation Error: Core 0 panic'ed (Stack protection fault)` in task `sys_evt` (ESP-IDF's system-event dispatch task, `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`, default 2304 bytes). Root cause: `log/handler.c`'s `on_log_message()` runs synchronously on **whichever task calls `logger->error/warn/info/debug(...)`** (by design, per its own doc comment) and stack-allocates a ~184-byte `pres_ble_handler_log_queue_item_t` there; unlike the settings/wifi_manager fix (bug #6), this buffer can't be moved into the handler struct because `on_log_message` is invoked concurrently from arbitrary tasks with no lock, so a shared buffer would risk corrupting log messages instead of fixing the crash. The real problem is that `sys_evt`'s stack was already nearly exhausted by a deep WiFi-disconnect → MQTT transport-error → esp-tls call chain before the logger call ever added its own frame; 4096 bytes was still not enough under a cascading multi-error burst, 8192 was. | **Fixed** — bumped `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` (`sdkconfig.defaults` and `sdkconfig`) from 2304 to 8192. Verified: repeated WiFi disconnect/reconnect via the BLE `wifi_manager` command characteristic with log forwarding enabled and subscribed produced zero crashes across multiple runs (had reliably crashed at both 2304 and 4096). |

## 1 — Gaps found during scenario execution

(filled in per scenario as the checklist above is actually run)

| # | Area | Gap | Status |
|---|---|---|---|
| 1 | BLE advertising | ~~Every boot logs `[ble/host/start_advertising] Failed to set advertising fields: 4` (NimBLE `BLE_HS_EMSGSIZE`) immediately after `BLE_INIT`, before the NimBLE host reports itself started. Advertising likely doesn't start correctly as a result. Not investigated — BLE is explicitly out of scope for this test pass (no Bluetooth adapter available on this dev machine to verify any BLE behavior).~~ Superseded — see `09-known-gaps-summary.md` critical bug #5: this dev machine now has a working Bluetooth adapter, the root cause was found and fixed, and the full BLE GATT surface (settings, wifi_manager, log services) has since been tested against real hardware. See `scenario/10-ble-gatt-services.md`. | **Fixed** (see critical bug #5 above). |

## Summary

- 7 real bugs found and fixed during this pass, all firmware/build-side
  except the first (cross-repo):
  1. Registration blocked entirely by a backend validation charset
     mismatch against the device's own dotted version string.
  2. Device's own compile-time `PROJECT_VER` macro silently empty due to
     a CMake `project()`/component-processing ordering gotcha, corrupting
     every registration's `firmware_name`.
  3. OTA silently failed on every real (non-toy) dispatch because the
     firmware-URL buffer (256 bytes) was too small for real MinIO
     presigned URLs (400+ bytes with a full SigV4 query string),
     truncating the URL mid-signature.
  4. `is_connected` read stale after a backend-only restart because the
     device's `ONLINE` status publish wasn't retained while its `OFFLINE`
     LWT (on the same topic) was — an internal firmware inconsistency,
     fixed by retaining both.
  5. BLE advertising never started at all (31-byte legacy advertising
     payload overflow from an overlong device name) — the device was
     completely undiscoverable/unconnectable over BLE.
  6. BLE settings/wifi_manager GATT reads/writes crashed the device
     (stack protection fault on the `nimble_host` task) due to
     stack-local JSON/payload buffers — fixed by moving them to
     struct-owned storage.
  7. BLE log-forwarding crashed the device (stack protection fault on
     the `sys_evt` task) under a WiFi-disconnect/MQTT-error logging
     cascade — fixed by increasing that task's configured stack size.
- 0 known gaps remaining unfixed from this pass — the one previously
  flagged BLE gap was resolved once a Bluetooth adapter became available
  (see bug #5).
- 38 of 42 WiFi/MQTT/OTA test cases executed and passing; the 4 unchecked
  (`WIFI-03`, `STAT-02`, `OTA-07`, `RESIL-01`/`02`/`04`) were skipped
  because they require physical AP power-cycling, abrupt USB power loss,
  or a deliberately-broken build — none controllable remotely in this
  environment — annotated as skipped, not failed, in the checklist.
- Verified end-to-end against the real device + real backend + real
  broker: cold-boot with blank NVS (defaults, no auto-connect), NVS
  seeding/persistence across reflash and OTA, device ID stability, WiFi
  connect/disconnect logging (including a wrong-password negative case),
  MQTT registration (including the no-matching-firmware negative case),
  ONLINE status, log-over-MQTT observability across a reconnect, the
  full action-dispatch matrix (all edge/negative cases: omitted
  delay_ms, missing execution_id, missing action field, unknown action
  name), and the full OTA matrix (successful cycle, checksum-mismatch
  rejection, unreachable-URL handling), backend-restart resilience
  (which surfaced gap #2 above), and — new this pass — the full BLE GATT
  surface (settings, wifi_manager, log services) against a real central,
  see `scenario/10-ble-gatt-services.md`.
