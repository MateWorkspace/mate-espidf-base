# 00 — Environment Setup

## Purpose

Get the real ESP32-C3 device running this firmware, connected to a real
WiFi AP and the real backend/MQTT broker described in `mate-things/.env`, so
every other scenario file can just assume the device is up and reachable.

Like the backend's own test pass, **this has no test/staging isolation** —
the WiFi AP, MQTT broker, and backend are shared resources. Data created
during this pass (node rows, action logs, firmware rows) will persist on
the backend. Re-running parts of this suite may produce duplicate/updated
rows rather than fresh ones — that's expected, not a bug.

## Tools required

- `idf.py` (ESP-IDF v6.0.2): `source /home/dhonan-wsl/.espressif/tools/activate_idf_v6.0.2.sh`
- Serial access to `/dev/ttyACM0` (user must be in the `dialout` group)
- `python3` with `requests` and `pyserial` installed
- `curl`, `mosquitto_pub`/`mosquitto_sub`
- The `mate-things` backend running and reachable — confirm with:
  ```bash
  curl -s -o /dev/null -w "%{http_code}\n" http://192.168.18.192:8080/api/v1/auth/login \
    -X POST -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"ChangeMe123!"}'
  # expect 200
  ```

## Step 1 — Seed NVS with test WiFi/MQTT credentials

There is no BLE provisioning path usable here (no Bluetooth adapter on this
machine), so credentials go in via a dedicated one-shot build instead — see
`main/Kconfig.projbuild` / `main/src/composition/test_seed/seed.c` for how
this works and why it's safe (`idf.py flash` never touches the `nvs`
partition, only bootloader/partition-table/app regions).

The seeded values (hardcoded in `seed.c`, safe to edit if the AP/broker
changes): WiFi SSID `Kartono internet` / password `KamiGendut123`; MQTT
`mqtts://e7c3d891e00d426bbab8a7b33fcd079e.s1.eu.hivemq.cloud:8883`, user
`matemqtt` — copied from `mate-things/.env`.

```bash
source /home/dhonan-wsl/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dhonan-wsl/Repositories/mate/mate-espidf-base

idf.py -B build_test_seed \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.test_seed" \
  -D SDKCONFIG="build_test_seed/sdkconfig" \
  build
idf.py -B build_test_seed -p /dev/ttyACM0 flash
```

Monitor until the seed log line appears, then stop (`Ctrl+]` if using
`idf.py monitor`, or kill the script doing the serial read):

```bash
python3 -c "
import serial, time, sys
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
end = time.time() + 8
while time.time() < end:
    line = ser.readline()
    if line:
        sys.stdout.write(line.decode(errors='replace')); sys.stdout.flush()
ser.close()
"
```

Expected: `Set mqtt_proto` / `Set mqtt_host` / ... / `Set wifi_sta_credential`
lines, then `Seed complete. Reflash the normal firmware now.`

## Step 2 — Flash the real firmware

```bash
idf.py build   # normal build, build/ directory, CONFIG_MATE_TEST_SEED_NVS_ON_BOOT=n by default
idf.py -p /dev/ttyACM0 flash
```

This does **not** touch the `nvs` partition — the credentials from Step 1
are still there.

## Step 3 — Get a backend access token

Same bootstrap user as the backend's own test pass: username `admin`,
password `ChangeMe123!`.

```bash
LOGIN_RESPONSE=$(curl -s -X POST http://192.168.18.192:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"ChangeMe123!"}')
ACCESS_TOKEN=$(echo "$LOGIN_RESPONSE" | jq -r .access_token)
```

Note `BE_TOKEN_ACCESS_DURATION=2m` in `.env` — re-login if a scenario takes
longer than 2 minutes between token use.

## Step 4 — Upload a firmware binary (needed for registration + OTA scenarios)

```bash
python3 upload.py
```

Reads `upload.config.json`, logs in, resolves the `base_node` node class
(created by the backend's seeder), and creates-or-replaces a firmware row
named to exactly match what the device's own registration payload sends
(`<PROJECT_NAME>_<PROJECT_VERSION>`, currently
`mate-espidf-base_v1.0.0-dev.1` — **must stay in sync with
`upload.config.json`'s `firmware_name` if either the project name or
version string ever changes**, or registration will silently fail, see
`09-known-gaps-summary.md`). Prints the firmware id and a freshly-issued
presigned download URL for OTA testing.

## Step 5 — Get the device's MQTT identity

Device ID is the 12-hex-char MAC-derived string, visible in the boot log
(`BLE_INIT: Bluetooth MAC: ...` line's last 6 bytes, no separators, or
directly from the settings/registration log lines):

```bash
DEVICE_ID="AC276E5E030C"   # replace with this device's actual ID from boot log
```

## MQTT credentials (shared across scenario files)

```bash
ENV_FILE=/home/dhonan-wsl/Repositories/mate/mate-things/.env
MQTT_HOST=$(grep BE_MQTT_BROKER_URL "$ENV_FILE" | sed -E 's#.*://([^:]+):.*#\1#')
MQTT_PORT=8883
MQTT_USER=$(grep BE_MQTT_USERNAME "$ENV_FILE" | cut -d= -f2)
MQTT_PASS=$(grep BE_MQTT_PASSWORD "$ENV_FILE" | cut -d= -f2)
```

Common flags for every `mosquitto_pub`/`mosquitto_sub` command below:
```
-h $MQTT_HOST -p $MQTT_PORT --cafile /etc/ssl/certs/ca-certificates.crt -u "$MQTT_USER" -P "$MQTT_PASS"
```

## Monitoring the device

There's no `idf.py monitor` session kept open across scenario files in this
checklist (it would block shell automation) — instead, each scenario that
needs to observe device-side logs uses a short timed Python serial capture
like Step 1's, reset via DTR/RTS toggle when a fresh boot is needed:

```python
import serial, time
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
ser.dtr = False; ser.rts = True; time.sleep(0.1); ser.rts = False; time.sleep(0.1)
# then read for N seconds as in Step 1
```

## Cleanup

Nothing device-side to clean up (NVS/flash state is meant to persist for
reuse). Backend-side: node rows, action logs, and the uploaded firmware row
persist in the shared dev Postgres/MinIO — no automated cleanup, matching
the backend's own test-pass precedent.
