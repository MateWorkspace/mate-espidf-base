# 05 — Action Dispatch

Precondition: device registered, `NODE_ID` (the backend's node UUID, not
`DEVICE_ID`) resolved:
```bash
NODE_ID=$(curl -s "http://192.168.18.192:8080/api/v1/nodes/by-device/$DEVICE_ID" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq -r .id)
ACTION_ID=$(curl -s "http://192.168.18.192:8080/api/v1/actions/by-name/restart" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq -r .id)
```
(`restart` is seeded by the backend against `base_node` by default — see
`database/seeder/action.json` — and is the **only** action this firmware
implements, `pres_mqtt_handler_action.c` hardcodes `action:"restart"` as the
sole recognized value.)

---

### ACT-01 — Dispatch reaches the device (positive)
```bash
mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT --cafile /etc/ssl/certs/ca-certificates.crt \
  -u "$MQTT_USER" -P "$MQTT_PASS" -t "/sub/$DEVICE_ID/action" -v &

curl -s -X POST "http://192.168.18.192:8080/api/v1/actions/$ACTION_ID/dispatch" \
  -H "Authorization: Bearer $ACCESS_TOKEN" -H "Content-Type: application/json" \
  -d "{\"node_id\":\"$NODE_ID\",\"payload\":{\"delay_ms\":5000}}"
```
**Expect:** the subscriber receives
`{"execution_id":"<uuid>","action":"restart","payload":{"delay_ms":5000}}`.
Capture `EXECUTION_ID` from the curl response for ACT-08.

### ACT-02 — Immediate SUCCESS ack, before restarting (positive)
```bash
mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT --cafile /etc/ssl/certs/ca-certificates.crt \
  -u "$MQTT_USER" -P "$MQTT_PASS" -t "/pub/$DEVICE_ID/action_ack" -v &
```
Re-dispatch as in ACT-01.
**Expect:** `{"execution_id":"...","status":"SUCCESS"}` arrives
**immediately** (well before the 5s `delay_ms` elapses) — `pres_mqtt_handler_
action.c` ACKs first, then schedules the actual restart, matching the "ack
regardless of eventual outcome" pattern the backend's own action-log design
expects.

### ACT-03 — Device actually restarts after `delay_ms`, re-registers (positive)
Capture serial log across the dispatch.
**Expect:** ~5s after ACT-01/02's dispatch, boot log reappears (reset
banner), followed by a fresh `/pub/registration` (confirm via a concurrent
`mosquitto_sub -t /pub/registration`).

### ACT-04 — `delay_ms` omitted defaults to 0 (edge)
```bash
curl ... -d "{\"node_id\":\"$NODE_ID\",\"payload\":{}}"
```
**Expect:** ACK still `SUCCESS`, device restarts promptly (no meaningful
delay) rather than erroring on the missing field.

### ACT-05 — Missing `execution_id` (negative)
This requires publishing directly to `/sub/$DEVICE_ID/action` rather than
via the backend (the backend always includes `execution_id`):
```bash
mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT --cafile /etc/ssl/certs/ca-certificates.crt \
  -u "$MQTT_USER" -P "$MQTT_PASS" -t "/sub/$DEVICE_ID/action" \
  -m '{"action":"restart"}'
```
**Expect:** dropped silently by the device (error logged locally, visible
via `04-logging.md`'s MQTT log stream), **no ack published at all** — matches
`pres_mqtt_handler_action.c` requiring `execution_id` before it can even
construct a valid ack.

### ACT-06 — Missing `action` field (negative)
```bash
mosquitto_pub ... -t "/sub/$DEVICE_ID/action" -m '{"execution_id":"test-06"}'
```
**Expect:** ack published with `status:"FAILED"`,
`message:"missing action field"`.

### ACT-07 — Unknown action name (negative, documents single-action limitation)
```bash
mosquitto_pub ... -t "/sub/$DEVICE_ID/action" \
  -m '{"execution_id":"test-07","action":"factory_reset"}'
```
**Expect:** ack `status:"FAILED"`, `message:"unknown action"` — there is
currently no device-side implementation for anything other than `restart`,
even if the backend has other actions seeded/defined for this node class.

### ACT-08 — Backend action log reflects device ack (positive, cross-check)
```bash
curl -s "http://192.168.18.192:8080/api/v1/action-logs?action_id=$ACTION_ID" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq '.data[] | {id, action_status, action_message}'
```
**Expect:** the `EXECUTION_ID` from ACT-01 shows `action_status: "SUCCESS"`
(from ACT-02's ack, which is genuine — not to be confused with backend-only
MQTT-driven test rows from the backend's own `16-mqtt-flows.md` suite).
