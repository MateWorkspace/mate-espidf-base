# mate-espidf-base

## Project goals

`mate-espidf-base` is the ESP32-C3 firmware paired with the `mate-things`
fleet-management backend. It provides:

- WiFi STA provisioning, persistence, status callbacks, and reconnect handling.
- MQTT registration, online/offline status, action dispatch, OTA, dynamic
  configuration, and device-log forwarding.
- BLE GATT services for settings, WiFi management, logs, and system/config
  information.
- NVS-backed preloaded configuration and an optional test-seed boot mode.
- OTA validation/rollback and firmware upload tooling.
- UTC clock synchronization through ESP-IDF SNTP so forwarded log timestamps
  become trustworthy after network time is acquired.

The target and hardware-in-loop reference device are ESP32-C3. The repository
currently builds against ESP-IDF v6.0.2.

## Project structure

```
mate-espidf-base/
├── CMakeLists.txt                 ESP-IDF project (`mate-espidf-base`)
├── version.txt                    firmware version used by ESP-IDF
├── sdkconfig.defaults             tracked project defaults
├── partitions/                    4/8/16 MB, OTA, and LittleFS layouts
├── main/
│   ├── include/                   public/internal headers by layer
│   ├── src/                       implementations mirroring include/
│   ├── CMakeLists.txt             recursive source registration + dependencies
│   ├── Kconfig.projbuild          project-specific Kconfig options
│   └── idf_component.yml          managed component dependencies
├── upload.py                      uploads the current build to mate-things
├── upload.config.json.example     non-secret upload configuration template
└── docs/
    ├── agent_test/                hardware-in-loop release scenarios
    ├── releases/                  release notes
    └── superpowers/               historical design specs and plans
```

`build/`, generated `sdkconfig*`, managed components, local
`upload.config.json`, and Python caches are local artifacts and must not be
committed.

### Firmware layers (`main/include` + `main/src`)

The source tree uses the same layered architecture in both header and source
roots:

```
presentation → application → domain ← infrastructure
                                ↑
                          composition
```

- **`domain/models/`** — shared value types, X-macro enum/schema definitions,
  and the project error model.
- **`domain/contracts/`** — interfaces for device, logger, messaging,
  repository, and system facilities. Infrastructure implements these;
  application code consumes them.
- **`domain/usecases/internal/`** — transport-independent firmware usecase
  interfaces such as settings, WiFi management, OTA, system info, messaging
  callbacks, and log forwarding.
- **`application/internal/`** — business/usecase implementations. This layer
  coordinates contracts and owns policy such as writable dynamic-config keys.
- **`infrastructure/`** — ESP-IDF/NVS/MQTT/logger/system implementations of
  domain contracts. Stub messaging implementations also live here for
  controlled composition/testing.
- **`presentation/mqtt/`** — MQTT context, event dispatch, wire DTO parsing,
  and handlers for registration ack, action, OTA, and config.
- **`presentation/ble/`** — NimBLE host, central GATT registry/UUID source of
  truth, and settings/WiFi/log/system-info handlers.
- **`presentation/task/`** — presentation-owned FreeRTOS background tasks,
  including WiFi reconnect supervision.
- **`composition/main/`** — dependency wiring only. `driver.c`,
  `infrastructure.c`, `application.c`, and `presentation.c` initialize in that
  order and deinitialize in reverse order through `launcher.c`.
- **`composition/test_seed/`** — alternate boot composition that writes
  controlled NVS values for hardware testing.
- **`composition/infrared/`** — alternate boot composition, a full
  duplicate of `composition/main/` with `infrastructure/device/ir` and
  `application/internal/infrared` additionally wired in. The only
  composition that subscribes to `ir/tx`. Selected the same way as
  `composition/counter_test/` — by editing the include/call in
  `main/main.c` — not by Kconfig. `composition/main` is the default active
  launcher; `composition/infrared` ships commented out until a build is
  explicitly targeting IR-capable hardware.

`main/main.c` selects the normal launcher unless
`CONFIG_MATE_TEST_SEED_NVS_ON_BOOT` is enabled.

## Core conventions

### Module lifecycle

Stateful modules follow `_new` / `_init` / `_deinit` / `_delete`:

- `_new(cfg)` validates configuration and allocates/copies state without
  registering callbacks or starting I/O.
- `_init(self)` performs wiring/registration and is idempotent.
- `_deinit(self)` symmetrically unregisters callbacks/tasks/handlers and is
  safe when already stopped.
- `_delete(self)` calls `_deinit` before freeing owned state.

Composition owns construction order and reverse teardown. If initialization
adds a process-global ESP-IDF service, unwind it on every later failure path
and in the matching composition deinit. SNTP currently lacks this cleanup;
add `esp_netif_sntp_deinit()` before introducing same-process composition
restart or lifecycle testing.

### Configuration validation

Each module with a configuration struct uses a dedicated `validate_cfg()`
helper (usually in `utils.c`) before allocation. Validate required pointers
and invariants at the boundary; do not defer null-pointer failures into event
callbacks.

### Logging tags and levels

Files using the domain logger define:

```c
#define BASE_TAG "<module/path>"
```

and use a per-function tag such as
`const char* tag = BASE_TAG "/init";`. `composition/main/launcher.c` and
`composition/test_seed/seed.c` are intentional exceptions: they run around or
before the domain logger and use ESP-IDF `ESP_LOGx` with a file-level `TAG`.

The only domain log levels are `NONE`, `ERROR`, `WARN`, `INFO`, and `DEBUG`,
defined by `DOMAIN_MODELS_LOGGER_LEVEL` in
`main/include/domain/models/logger.h`. Do not add `VERBOSE` in one transport
only.

The stdio logger emits exactly:

```
DD/MM/YYYY HH:MM:SS.mmm [LEVEL] [TAG] message
```

The backend parser depends on this shape. No timezone is configured, so
timestamps are UTC. SNTP is asynchronous: early boot logs may still contain
1970 timestamps before synchronization completes. The
`SNTP time sync (re)triggered` message means restart was accepted, not that
time synchronization has completed.

### Error handling

Public project APIs return `dom_models_error_t` values from
`domain/models/error.h`; translate ESP-IDF errors at infrastructure or
composition boundaries. Log an error once at the layer that has useful
context, then propagate it. Do not silently swallow registration,
allocation, parsing, or teardown failures.

### Callback and task memory safety

ESP-IDF event tasks and NimBLE GATT callbacks can have shallow stacks:

- Avoid meaningful stack-local buffers in MQTT/BLE/event callbacks. Prefer
  object-owned, static-lifetime, or heap-backed storage with named size
  constants.
- Check payload lengths before copying and preserve explicit NUL termination
  for strings.
- Keep network/time synchronization non-blocking from event handlers.
- Do not perform slow work while holding callback/event-loop context; hand it
  to the appropriate task or ESP-IDF subsystem.

Past hardware testing found stack-protection faults in this area. A current
known gap remains in the WiFi-disconnect/MQTT-error cascade: issuing the BLE
WiFi `DISCONNECT` command can overflow the `sys_evt` task and reboot before a
same-process reconnect command is sent. Treat disconnect/reconnect testing as
potentially disruptive until that separate bug is fixed.

### DTO and wire-boundary ownership

Transport handlers own wire parsing/encoding in focused `dto.c`/`dto.h`
files. Application/domain layers must not depend on cJSON, NimBLE request
objects, MQTT topic buffers, or transport-specific framing.

Keep BLE UUIDs centralized in `presentation/ble/gatt/uuid.{c,h}`. Do not
hand-code byte-reversed UUID literals inside individual handlers.

## Protocol contracts

### MQTT

- Publish registration on `/pub/registration`.
- Publish per-device status and logs beneath `/pub/<device_id>/...`.
- Subscribe per device to:
  - `/sub/<device_id>/registration_ack`
  - `/sub/<device_id>/action`
  - `/sub/<device_id>/ota`
  - `/sub/<device_id>/config`
  - `/sub/<device_id>/ir/tx` (only subscribed by the `composition/infrared`
    build variant)
- Publish IR captures on `/pub/<device_id>/ir/rx` and transmit results on
  `/pub/<device_id>/ir/tx_ack`, both only from `composition/infrared`.

Build topic strings through existing messaging/context helpers; preserve the
leading slash and exact suffixes because the backend subscriptions use the
same contract. Device IDs are derived from the MAC and formatted as the
stable uppercase hexadecimal string used throughout the project.

`/sub/<device_id>/registration_ack`'s payload is `{"success": bool}` (see
`presentation/mqtt/handler/registration_ack/`, folder-per-handler with a
DTO like `action`/`config`/`ota`). `success:true` logs and proceeds
normally. Every failure mode — `success:false`, a missing `success` field,
or an unparseable payload — logs an error and calls
`ctx->messaging_callbacks->restart(ctx->messaging_callbacks, 0)` (delay 0 =
immediate `esp_restart()`), not just an explicit `false`. Keep this in sync
with the backend's `messaging_callback` usecase — see the root
`mate-things/AGENTS.md`'s cross-layer fleet contracts.

### Dynamic configuration

`DOMAIN_MODELS_PRELOADED_SCHEMA` in
`main/include/domain/models/preloaded.h` is the single source of truth for
preloaded keys and their types. Update the X-macro and its consumers together;
do not duplicate schema tables by hand.

Not every BLE-writable setting is automatically writable over MQTT. Preserve
the application-layer writable-key policy and the boot-loop guard for
`sys_rst_aft_ms`.

### Firmware identity and upload

The project name comes from the root `project(...)`; the version comes from
`version.txt`. `main/CMakeLists.txt` deliberately exports ESP-IDF's resolved
`PROJECT_VER` as a compile-time definition so MQTT registration and the boot
banner agree. Do not hand-type `firmware_name`.

`upload.py` derives firmware identity and the preloaded config schema from
the source tree, then uses local `upload.config.json`. Never commit that file
or credentials; update `upload.config.json.example` when the public config
shape changes.

## Development flow

### ESP-IDF environment

For the current development machine:

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
```

Do not mix build artifacts from another ESP-IDF version.

### Configure and build

```bash
idf.py reconfigure
idf.py build
```

`main/CMakeLists.txt` uses `GLOB_RECURSE`; run `idf.py reconfigure` whenever
source files are added, removed, or moved so CMake refreshes the source list.
For edits to existing files, `idf.py build` is normally sufficient, but the
final verification should include reconfigure.

Format touched C/C++ files with `clang-format` using the repository
`.clang-format`. Do not reformat unrelated files. `git diff --check` must be
clean.

### Flash and monitor

The usual connected device is `/dev/ttyACM0`:

```bash
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
```

Use bounded monitoring and exit cleanly so the serial port is released.
Normal `flash` preserves NVS; `erase-flash` is destructive and requires an
explicit reason and a plan to restore credentials/configuration.

### Test-seed build

There is no host/unit-test framework for the firmware today. For controlled
NVS seeding and the exact alternate-build commands, follow
`docs/agent_test/v1.0.0-dev.1/scenario/00-setup.md`. Reflash the normal build
after the seed composition reports completion.

### Verification checklist

1. Source the ESP-IDF v6.0.2 environment.
2. Run `idf.py reconfigure && idf.py build`; introduce no new warnings.
3. Confirm formatting and `git diff --check`.
4. For runtime changes, flash the real device and watch a bounded clean boot:
   no reset loop, stack-protection fault, or new error.
5. Exercise the relevant MQTT/BLE/WiFi/OTA behavior using the matching
   `docs/agent_test/v1.0.0-dev.1/scenario/` checklist and real backend/broker
   where required.
6. For log/time changes, confirm a hardware-originated row in the backend has
   the intended parsed level/tag/message and a current UTC timestamp. Avoid
   leaving synthetic rows or altered device configuration behind.

Build success alone is not enough for event, radio, persistence, OTA, or
cross-repository protocol changes.

## Managed dependencies and generated state

- Built-in ESP-IDF components belong in `PRIV_REQUIRES` in
  `main/CMakeLists.txt`.
- Registry-managed components belong in `main/idf_component.yml`; let ESP-IDF
  update `dependencies.lock`.
- Prefer built-in components over adding a managed dependency for facilities
  ESP-IDF already provides.
- Edit tracked defaults/Kconfig inputs, not generated `sdkconfig` or build
  files.
- Partition changes must update the appropriate file under `partitions/` and
  be verified against the actual flash size and OTA requirements.
