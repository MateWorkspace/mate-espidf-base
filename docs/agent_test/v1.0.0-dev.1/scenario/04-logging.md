# 04 — Logging Observability

Precondition: device registered (`03-mqtt-registration-status.md` REG-01/02
passing), `DEVICE_ID`/MQTT env vars available.

---

### LOG-01 — Log lines observable over MQTT (positive)
```bash
mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT --cafile /etc/ssl/certs/ca-certificates.crt \
  -u "$MQTT_USER" -P "$MQTT_PASS" -t "/pub/$DEVICE_ID/log" -v
```
**Expect:** a steady trickle of raw (non-JSON) log lines matching what's
also printed to the serial console — every `logger->error/warn/info/debug`
call is forwarded here via `messaging_callbacks`' subscription to the
logger contract's `add_callback` (same fan-out mechanism the BLE log
handler also uses, per the prior BLE session — though BLE itself isn't
being tested here).

### LOG-02 — Logging continues through a WiFi reconnect (positive)
While the `mosquitto_sub` from LOG-01 is running, trigger a WiFi
disconnect/reconnect (power-cycle the AP briefly).
**Expect:** log lines about the disconnect/reconnect itself
(`internal_wifi_manager`'s connect/disconnect logging from `02-wifi-
connection.md`) also arrive over MQTT once the connection is back — confirms
the log→MQTT path doesn't get stuck/deadlocked by a WiFi state change, and
that logs generated *while* WiFi is down are not silently lost forever (MQTT
client should buffer/retry, though very early logs generated before the
first-ever connection obviously can't retroactively appear here — that's
expected, not a bug).
