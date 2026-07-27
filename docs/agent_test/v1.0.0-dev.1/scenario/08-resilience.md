# 08 — Resilience & Reconnect

Precondition: device registered and connected.

---

### RESIL-01 — MQTT broker reconnect (positive)
There's no direct way to bounce just the client's connection to HiveMQ
Cloud from here; approximate it by toggling WiFi off/on (`02-wifi-
connection.md` WIFI-03), which forces the MQTT client to reconnect once IP
connectivity returns.
**Expect:** MQTT client reconnects automatically (`network.reconnect_
timeout_ms` equivalent on the device side / broker-side keepalive), status
goes back to `is_connected: true`.

### RESIL-02 — Per-device subscriptions survive reconnect (positive)
After RESIL-01's reconnect, re-run `05-action-dispatch.md` ACT-01.
**Expect:** action dispatch still reaches the device — confirms
`on_connect.c`'s `subscribe_defaults_impl()` (registration_ack/ota/action
topics) re-subscribes correctly on every connect event, not just the first
one ever.

### RESIL-03 — Backend restart mid-session (positive)
```bash
docker restart mate-things-app-1
sleep 8
```
**Expect:** once the backend container is back up, the device (which never
stopped trying) successfully reconnects/re-registers — confirm via
`curl .../v1/nodes/by-device/$DEVICE_ID` showing `is_connected: true` again
without any device-side intervention (no reset needed).

### RESIL-04 — WiFi AP restart, watchdog reconnect (positive)
Power-cycle the AP itself (not just move the device out of range).
**Expect:** `presentation/task/wifi_sta_reconnect`'s background task
(polls `need_reconnect()`/`try_reconnect()` every
`PRES_TASK_WIFI_STA_RECONNECT_DEFAULT_INTERVAL_MS` = 5000ms) picks up the
STA disconnect and reconnects once the AP is back, without needing a
device power cycle or manual `idf.py flash`.
