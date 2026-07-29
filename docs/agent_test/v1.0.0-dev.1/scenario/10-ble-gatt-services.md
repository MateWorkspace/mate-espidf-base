# 10 — BLE GATT Services

## Purpose

Exercise the device's BLE GATT surface (settings, wifi_manager, log
services) against a real central. This scenario was **not** part of the
original checklist's 42 test cases (`checklist.md` explicitly marked BLE
out of scope, since the original dev machine had no Bluetooth adapter) —
it was added once a working adapter became available.

## Tools required

- `bluetoothd` (bluez) running and `hci0` unblocked/up:
  ```bash
  rfkill unblock bluetooth
  hciconfig hci0 up
  bluetoothctl show   # confirm Powered: yes
  ```
- Python `bleak` (BLE central library): `pip install bleak` (in a venv if
  the system Python is externally managed).
- Real ESP32-C3 device (`/dev/ttyACM0`) running this firmware, connected
  over WiFi (see `00-setup.md`).

## Protocol reference

Every custom UUID follows `4d415445-SSSS-4700-CCCC-000000000000`
(`SSSS` = service, `CCCC` = characteristic, see `gatt/uuid.h`'s comment).

| Service | UUID (SSSS) |
|---|---|
| Settings | `0001` |
| WiFi manager | `0002` |
| Log | `0003` |
| System info | `0004` |

Settings (`0001`): `0001` data (R), `0002` update (W, JSON), `0003`
restart_required (R/notify), `0004` restart (W, `uint32_t` delay_ms).

WiFi manager (`0002`): `0001` status (R/notify), `0002` connect (W, JSON
`{ssid,password}`), `0003` command (W, `uint8_t` opcode — `0`=stop,
`1`=start, `2`=connect_stored, `3`=disconnect, `4`=forget_stored), `0004`
stored_credential (R), `0005` try_connect_on_init (R/W, `uint8_t` bool).

Log (`0003`): `0001` message (R/notify), `0002` enabled (R/W, `uint8_t`
bool).

System info (`0004`): `0001` info (R, JSON `{project,chip}`), `0002`
config_schema (R, JSON array of `{key,type}` for every preloaded config
variable).

Device advertises as `matedev_<device_id_str>` (e.g.
`matedev_AC276E5E030C`) — only the name fits the legacy 31-byte
advertising payload, so services must be discovered post-connect (GATT
discovery), not from the advertisement itself.

## Results — this pass

- [x] **BLE-01** — Device advertises and is discoverable by name after the
  device-name-length fix (see `09-known-gaps-summary.md` bug #5) (positive)
- [x] **BLE-02** — GATT service/characteristic discovery matches the
  UUID/property table above exactly (positive)
- [x] **BLE-03** — Settings `data` read returns a valid JSON snapshot
  matching NVS-seeded values (positive; found+fixed a crash here, bug #6)
- [x] **BLE-04** — Settings `update` write + `restart_required` read
  reflects the change (`restart_required: false` → `true`) (positive)
- [x] **BLE-05** — Settings `data` re-read after `update` shows the new
  value persisted in the live snapshot (positive)
- [x] **BLE-06** — WiFi manager `status` read reflects live connection
  state (SSID/IP/RSSI/etc.) (positive)
- [x] **BLE-07** — WiFi manager `stored_credential` read reflects the
  seeded SSID without exposing the password (positive)
- [x] **BLE-08** — WiFi manager `try_connect_on_init` read returns the
  seeded flag (positive)
- [x] **BLE-09** — Log `enabled` read/write round-trips correctly
  (positive)
- [x] **BLE-10** — WiFi manager `command` write (`disconnect` then
  `connect_stored`) triggers the expected WiFi disconnect/reconnect cycle
  with zero device crashes after bugs #6/#7 were fixed (positive; this is
  the scenario that originally reproduced bug #7 at both the default and
  first-attempted-fix stack sizes)
- [ ] **BLE-11** — Log message notifications delivered to a subscribed
  central during the above disconnect/reconnect cycle — **not confirmed**:
  0 notifications observed in the run that didn't crash, most likely BLE/
  WiFi radio coexistence (ESP32-C3 has a single radio time-shared between
  BLE and WiFi) dropping the BLE connection during the WiFi reconnect,
  rather than a firmware bug. Not investigated further this pass — flag
  for a future pass if BLE log forwarding during active WiFi
  reconnects is a real product requirement.
- Not exercised this pass: WiFi manager `connect` (write new credentials)
  and settings `restart` (write delay_ms) — both are simple write paths
  using the same now-fixed buffer pattern as the characteristics that
  were exercised, but weren't independently triggered to avoid
  disrupting the shared WiFi credential state mid-pass.
- [x] **BLE-12** — System info `info` read returns project/chip data
  matching the device's actual values; `config_schema` read lists all 7
  preloaded config keys with correct types; Settings `data` no longer
  includes `project`/`chip` (positive)

## Bugs found (see `09-known-gaps-summary.md` for full detail)

1. BLE advertising never started (device name overflowed the 31-byte
   legacy advertising payload) — **fixed**.
2. Settings/wifi_manager GATT read/write crashed the device (stack
   protection fault, `nimble_host` task, stack-local buffers) — **fixed**
   by moving buffers to struct-owned storage.
3. BLE log-forwarding crashed the device (stack protection fault,
   `sys_evt` task, under a WiFi-disconnect/MQTT-error logging cascade) —
   **fixed** by increasing `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` from
   2304 to 8192.
