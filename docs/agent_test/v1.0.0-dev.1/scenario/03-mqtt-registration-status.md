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
`{"device_id":"<DEVICE_ID>","device_info":"...","node_class_name":"base_node","firmware_name":"mate-espidf-base_v1.0.0-dev.1"}`.
`node_class_name` comes from the `MATE_NODE_CLASS_NAME` property set in the
root `CMakeLists.txt` (default `base_node`), injected at compile time as
`NODE_CLASS_NAME` — not from the matched firmware.

### REG-02 — Backend accepts registration, creates/finds a node (positive)
```bash
sleep 3
curl -s "http://192.168.18.192:8080/api/v1/nodes/by-device/$DEVICE_ID" \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```
**Expect:** `200` with a node row, `node_class_id` matching the node class
named `base_node` (resolved from the payload's `node_class_name`, which must
already exist on the backend — `database/seeder/node_class.json` seeds it).
If this returns `404`, check that the node class exists via
`curl .../v1/node-classes/by-name/base_node` before assuming a device-side
bug.

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

### REG-04 — Registration with no matching firmware (positive)
Temporarily change `firmware_name` in `upload.config.json` to something not
yet uploaded (or delete the uploaded firmware row via
`DELETE /v1/firmwares/{id}`), reset the device.
**Expect:** registration still succeeds — the node is created/updated with
`firmware_id` left `null` (an unmatched firmware name is no longer a
registration failure; only an unmatched `node_class_name` is). Confirm via
`curl .../v1/nodes/by-device/$DEVICE_ID` that the node row exists with
`firmware_id` absent/null. Restore the correct `firmware_name`/firmware row
afterward.

### REG-05 — Registration with no matching node class (negative)
Temporarily rename the `base_node` node class (or otherwise ensure no node
class matches `MATE_NODE_CLASS_NAME`'s value), reset the device.
**Expect:** matches backend's own MQTT-01 note — registration silently
fails server-side (`UpsertRegistration` → `pgx.ErrNoRows` → "node class not
found"), **no ack sent back, no error surfaced to the device at all**. The
device has no way to know registration failed. Restore the node class
afterward.

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
