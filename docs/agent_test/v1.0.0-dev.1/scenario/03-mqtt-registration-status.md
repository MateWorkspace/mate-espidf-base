# 03 — MQTT Registration & Status

Precondition: `00-setup.md` complete, including Step 4 (firmware uploaded
with a name matching the device's registration payload). `ACCESS_TOKEN`,
`DEVICE_ID`, and the MQTT env vars from `00-setup.md` available.

---

### REG-01 — Device publishes `/pub/registration` on connect (positive)
```bash
mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT --cafile /etc/ssl/certs/ca-certificates.crt \
  -u "$MQTT_USER" -P "$MQTT_PASS" -t "/pub/registration" -v &
# then reset the device
```
**Expect:** one message,
`{"device_id":"<DEVICE_ID>","device_info":"...","firmware_name":"mate-espidf-base_v1.0.0-dev.1"}`.

### REG-02 — Backend accepts registration, creates/finds a node (positive, ⚠ see known gap)
```bash
sleep 3
curl -s "http://192.168.18.192:8080/api/v1/nodes/by-device/$DEVICE_ID" \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```
**Expect:** `200` with a node row, `node_class_id` matching `base_node`.
⚠ **Only works because `upload.py` uploaded a firmware whose name exactly
matches the device's payload, and because the backend's firmware-name
validation was loosened this session to permit dots** — see
`09-known-gaps-summary.md`. If this returns `404`, first check the firmware
row's name via
`curl .../v1/firmwares/by-name/mate-espidf-base_v1.0.0-dev.1` before
assuming a device-side bug.

### REG-03 — `registration_ack` received, no-op (positive)
```bash
mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT --cafile /etc/ssl/certs/ca-certificates.crt \
  -u "$MQTT_USER" -P "$MQTT_PASS" -t "/sub/$DEVICE_ID/registration_ack" -v &
# reset the device again
```
**Expect:** the backend publishes an ack (per its own `messaging_callback`
usecase's `RegistrationAck` call after every successful registration); the
device receives it and logs `"Received registration ack via MQTT"` with
zero further effect — `pres_mqtt_handler_registration_ack` ignores the
payload entirely. Confirm via device log capture that nothing else happens.

### REG-04 — Registration with no matching firmware (negative)
Temporarily change `firmware_name` in `upload.config.json` to something not
yet uploaded (or delete the uploaded firmware row via
`DELETE /v1/firmwares/{id}`), reset the device.
**Expect:** matches backend's own MQTT-01 note — registration silently
fails server-side (`UpsertRegistration` → `pgx.ErrNoRows` → "firmware not
found"), **no ack sent back, no error surfaced to the device at all**. The
device has no way to know registration failed. Restore the correct
`firmware_name`/firmware row afterward.

### STAT-01 — Status ONLINE on connect (positive)
```bash
curl -s "http://192.168.18.192:8080/api/v1/nodes/by-device/$DEVICE_ID" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq .is_connected
```
**Expect:** `true`, shortly after boot.

### STAT-02 — LWT publishes OFFLINE on ungraceful disconnect (positive)
Power-cycle the device abruptly (unplug USB) rather than a clean reset.
```bash
sleep 5
curl -s "http://192.168.18.192:8080/api/v1/nodes/by-device/$DEVICE_ID" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq .is_connected
```
**Expect:** `false` — the MQTT broker publishes the device's configured LWT
(`{"status":"OFFLINE"}`, retained, qos 1, set in
`inf_messaging_def_pub_mqtt_impl`'s client config) once it detects the
dropped connection (may take up to the broker's keepalive timeout). Plug
the device back in afterward.
