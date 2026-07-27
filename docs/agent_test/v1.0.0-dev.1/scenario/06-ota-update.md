# 06 — OTA Update

Precondition: device registered, `NODE_ID`/`DEVICE_ID`/`ACCESS_TOKEN`
available. Note `BE_MINIO_PRESIGN_DURATION=5m` in `.env` — the download URL
`upload.py` prints expires 5 minutes after it's issued, so dispatch OTA
promptly after uploading.

---

### OTA-01 — `upload.py` create-or-replace (positive, tooling check)
```bash
idf.py build   # ensure build/mate-espidf-base.bin is current
python3 upload.py
```
**Expect:** prints `firmware_id`, `size`, `checksum`, `binary_path`, and a
presigned URL. Re-running immediately should print the **same**
`firmware_id` (replace-in-place via `PUT .../binary`, not a new row) with a
**different** presigned URL (freshly signed each call) — confirms the
create-vs-replace branch in `upload.py` works both ways.

### OTA-02 — OTA dispatch reaches the device (positive)
```bash
FIRMWARE_ID=$(curl -s "http://192.168.18.192:8080/api/v1/firmwares/by-name/mate-espidf-base_v1.0.0-dev.1" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq -r .id)
FIRMWARE_URL=$(python3 upload.py | grep '^http')   # re-issues a fresh presigned URL

mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT --cafile /etc/ssl/certs/ca-certificates.crt \
  -u "$MQTT_USER" -P "$MQTT_PASS" -t "/sub/$DEVICE_ID/ota" -v &

curl -s -X POST "http://192.168.18.192:8080/api/v1/nodes/by-device/$DEVICE_ID/ota" \
  -H "Authorization: Bearer $ACCESS_TOKEN" -H "Content-Type: application/json" \
  -d "{\"firmware_id\":\"$FIRMWARE_ID\",\"firmware_url\":\"$FIRMWARE_URL\"}"
```
**Expect:** subscriber receives
`{"firmware_url":"...","firmware_size":<int>,"firmware_checksum":"..."}` —
`firmware_size`/`firmware_checksum` are filled in server-side from the
firmware row (`node/ota` usecase reads them, not supplied by the caller),
so they should match `OTA-01`'s printed `size`/`checksum` exactly.

### OTA-03 — Device downloads, flashes, reboots (positive)
Capture serial log across the dispatch (may take up to a minute or two for
a ~1.3MB download+flash over WiFi).
**Expect:** log shows the OTA usecase's update flow (`esp_https_ota`
progress/completion), then a reset, then normal boot into the new image
(app_init's version/SHA banner should reappear — since this test uploads
the *same* source unchanged, the version string won't visibly differ, but
the reset + successful re-boot is the signal to look for; see OTA-07 for a
version-difference-visible variant if desired).

### OTA-04 — Post-OTA registration sanity check (positive)
```bash
sleep 5
curl -s "http://192.168.18.192:8080/api/v1/nodes/by-device/$DEVICE_ID" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq '{firmware_id, is_connected}'
```
**Expect:** `is_connected: true` again post-reboot; if the node model
tracks `firmware_id`, confirm it matches `OTA-01`'s `FIRMWARE_ID`.

### OTA-05 — Checksum mismatch rejected (negative)
Dispatch with a deliberately wrong `firmware_checksum` — this requires
publishing directly to `/sub/$DEVICE_ID/ota` (the backend always computes
the real checksum from the DB row, so this can't be triggered via the HTTP
API):
```bash
mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT --cafile /etc/ssl/certs/ca-certificates.crt \
  -u "$MQTT_USER" -P "$MQTT_PASS" -t "/sub/$DEVICE_ID/ota" \
  -m "{\"firmware_url\":\"$FIRMWARE_URL\",\"firmware_size\":$(stat -c%s build/mate-espidf-base.bin),\"firmware_checksum\":\"0000000000000000000000000000000000000000000000000000000000000000\"}"
```
**Expect:** `esp_https_ota`'s checksum verification (or the OTA usecase's
own check, depending on where it's implemented — confirm in code if this
doesn't behave as expected) rejects the update before it's applied; device
keeps running the current firmware, no crash, no boot into a corrupt image.

### OTA-06 — Unreachable URL (negative)
```bash
mosquitto_pub ... -t "/sub/$DEVICE_ID/ota" \
  -m '{"firmware_url":"http://192.168.18.192:8080/api/v1/firmwares/does-not-exist/binary","firmware_size":1000,"firmware_checksum":"abc"}'
```
**Expect:** download fails (404/connection error), logged, device continues
running normally — no OTA-failure ack topic exists in this firmware's
design (unlike `action_ack`), so failure is only observable via the device's
own log stream (`04-logging.md`), not via any backend-visible state change.

### OTA-07 — Rollback on validation failure (negative, ⚠ optional/destructive)
Requires a deliberately broken build (e.g. temporarily make `app_main`
crash or hang before `ota->validate()` runs) uploaded and dispatched via
OTA-02/03. **Time-consuming and requires careful recovery** — only run this
if there's time budget for it; otherwise leave unchecked in the checklist
and annotate as skipped, not failed.
**Expect (if run):** `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` (set in
`sdkconfig.defaults`) causes the bootloader to detect the failed
self-validation and roll back to the previously-running OTA partition on
next boot, without manual intervention.
