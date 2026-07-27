# 02 — WiFi Connection

Precondition: `00-setup.md` complete (NVS seeded, real firmware flashed).

---

### WIFI-01 — STA connects to the seeded AP (positive)
Reset the device, capture ~15s of boot log.
**Expect:** `wifi:` driver logs showing station start/connect, and an
`internal_wifi_manager/on_wifi_event` info line
`"WiFi connected, IP: <a.b.c.d>"` within a few seconds — confirms both the
connection itself and this session's new always-on connect/disconnect
logging feature (`app_internal_wifi_manager_impl.c`'s `on_wifi_event`).

### WIFI-02 — Connect-logging fires exactly once per connect (positive)
Count `"WiFi connected, IP:"` occurrences in a single boot's log.
**Expect:** exactly one, not one per retry/event (the handler only logs on
`DOM_MODELS_WIFI_EVENT_STA_GOT_IP`, skipping the earlier link-layer-only
`STA_CONNECTED` event specifically to avoid this).

### WIFI-03 — Disconnect logging (positive)
With the device connected and MQTT observed as up, power off the AP (or
move the device out of range), capture log for ~15s.
**Expect:** `"WiFi disconnected"` logged. Restore the AP afterward for
later scenarios.

### WIFI-04 — Wrong WiFi password (negative)
Temporarily edit `TEST_WIFI_PASSWORD` in
`main/src/composition/test_seed/seed.c` to something wrong, rebuild+reflash
the seed variant, then reflash the normal firmware, reset, capture log.
**Expect:** repeated connect failures logged (WiFi driver
disconnect/reconnect events), no crash, no restart loop. Revert the seed
value and re-seed afterward before continuing the checklist.

### WIFI-05 — `wifi_sta_try_connect_on_init=false` (negative/edge)
Boot with blank NVS (as in BOOT-01) but do **not** run `test_seed` at all —
just the normal firmware against erased flash.
**Expect:** no STA connect attempt at boot (default is `false`); confirms
the flag genuinely gates auto-connect rather than it happening
unconditionally whenever credentials happen to be present. Re-seed
afterward.
