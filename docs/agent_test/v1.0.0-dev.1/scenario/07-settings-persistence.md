# 07 — Settings Persistence

Precondition: `00-setup.md` complete.

---

### SET-01 — NVS matches what `test_seed` wrote (positive)
Cross-reference `01-boot-and-config.md` BOOT-02 and `02-wifi-connection.md`
WIFI-01 — if both pass, the MQTT proto/host/port/user/pass and WiFi
SSID/password round-tripped through NVS correctly (there is no direct "dump
NVS" tool used here; correctness is inferred from the device successfully
connecting to both the exact seeded AP and the exact seeded broker, which
would fail immediately on any typo/corruption).

### SET-02 — Survive a normal reflash (positive)
```bash
idf.py build && idf.py -p /dev/ttyACM0 flash
```
**Expect:** device reconnects to WiFi/MQTT immediately after, same as
before reflashing — re-confirms `BOOT-03`'s mechanism specifically from the
settings/credentials angle (as opposed to just "device still boots").

### SET-03 — Survive an OTA update (positive)
After `06-ota-update.md` OTA-03 completes and the device reboots into the
OTA-delivered image.
**Expect:** device reconnects to WiFi/MQTT and re-registers without needing
`test_seed` run again — confirms OTA (which only writes the `ota_0`/`ota_1`
app partitions via `esp_https_ota`) never touches the `nvs` partition
either, same guarantee as a normal `idf.py flash`.
