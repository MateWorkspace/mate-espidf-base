# 01 — Boot & NVS Preloaded Config

Precondition: device flashed with the normal firmware. For BOOT-01 only,
use a fully erased NVS (see that case for how).

---

### BOOT-01 — Cold boot with blank NVS (positive)
```bash
source /home/dhonan-wsl/.espressif/tools/activate_idf_v6.0.2.sh
idf.py -p /dev/ttyACM0 erase-flash
idf.py -p /dev/ttyACM0 flash
```
**Expect:** boots without crash/panic; log shows `mqtt_host=192.168.1.1`
(compiled-in default, see `preloaded.c`'s `load_default()`), no WiFi STA
connect attempt (since `wifi_try_init` default is `false`), MQTT client
starts but immediately fails to connect (default host is a private-network
placeholder, not a real broker) — this is expected, not a bug. Re-seed via
`00-setup.md` Steps 1–2 afterward before running any other scenario.

### BOOT-02 — Boot after `test_seed` (positive)
After `00-setup.md` Steps 1–2, capture the boot log (see Step "Monitoring
the device").
**Expect:** no `mqtt_proto`/`mqtt_host`/... log lines showing defaults —
`internal_wifi_manager`/`messaging_callbacks` behavior downstream should
reflect the seeded HiveMQ Cloud broker and `Kartono internet` AP, confirmed
indirectly via `WIFI-01`/`REG-01` succeeding.

### BOOT-03 — `idf.py flash` does not erase NVS (positive, mechanism check)
```bash
idf.py -p /dev/ttyACM0 flash
```
(without `erase-flash`, immediately after BOOT-02's seed).
**Expect:** device still connects to WiFi/MQTT with the seeded credentials
— confirms `flash_args` only writes bootloader/partition-table/app regions
(`0x0`, `0x8000`, `0xd000`, `0x10000`), never the `nvs` partition at
`0x9000` (see `partitions/4mb.csv`).

### BOOT-04 — Device ID stable across reboots (positive)
Reset the device twice (power cycle or DTR/RTS toggle), capture the MAC /
device-id-derived log line both times.
**Expect:** identical 12-hex-char device ID both times — it's derived from
`esp_base_mac_addr_get()`, not stored/regenerated in NVS.

### BOOT-05 — `system_restart_after_ms` sentinel (edge)
Default value is `0xFFFFFFFF` (`DEFAULT_SYSTEM_RESTART_AFTER_MS`).
`launcher.c` unconditionally calls
`settings->restart(..., dom_models_preloaded_data.system_restart_after_ms)`
after successful boot.
**Expect:** confirm in code / by observation that this huge delay value
does **not** cause a restart during any reasonable test session (effectively
"never" — `0xFFFFFFFF` ms ≈ 49.7 days). If the device unexpectedly restarts
on its own during later scenarios, check this value first before assuming a
crash.
