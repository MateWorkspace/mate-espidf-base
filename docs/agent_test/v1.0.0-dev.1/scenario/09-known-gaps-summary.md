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

## 1 — Gaps found during scenario execution

(filled in per scenario as the checklist above is actually run)

| # | Area | Gap | Status |
|---|---|---|---|
| 1 | BLE advertising | Every boot logs `[ble/host/start_advertising] Failed to set advertising fields: 4` (NimBLE `BLE_HS_EMSGSIZE`) immediately after `BLE_INIT`, before the NimBLE host reports itself started. Advertising likely doesn't start correctly as a result. Not investigated — BLE is explicitly out of scope for this test pass (no Bluetooth adapter available on this dev machine to verify any BLE behavior). | **Not fixed, out of scope.** Flag for a future pass once BLE can be tested with a real central. |

## Summary

- 4 real bugs found and fixed during this pass, all firmware/build-side
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
- 1 known gap flagged but not fixed (out of scope for this pass):
  BLE advertising `EMSGSIZE` on startup — no Bluetooth adapter available
  to verify BLE behavior in this environment.
- 38 of 42 test cases executed and passing; the 4 unchecked
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
  rejection, unreachable-URL handling), and backend-restart resilience
  (which surfaced gap #2 above).
