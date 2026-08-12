# mate-espidf-base

ESP32-C3 firmware for devices in the `mate-things` IoT fleet-management
platform. It handles WiFi provisioning, MQTT registration/status/action
dispatch/OTA/dynamic config, BLE GATT settings, NVS-backed preloaded
configuration, OTA validation/rollback, and log forwarding to the backend.

Builds against **ESP-IDF v6.0.2**. See [`AGENTS.md`](./AGENTS.md) for the
full architecture, layering, and development conventions.

## Environment

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
```

Don't mix build artifacts from another ESP-IDF version.

## Build

```bash
idf.py reconfigure
idf.py build
```

`main/CMakeLists.txt` uses `GLOB_RECURSE`, so run `idf.py reconfigure`
whenever source files are added, removed, or moved so CMake picks up the
new source list. For edits to existing files, `idf.py build` alone is
normally enough.

## Flash

```bash
idf.py -p /dev/ttyACM0 flash
```

Normal `flash` preserves NVS (WiFi credentials, preloaded config, etc.).
`idf.py -p /dev/ttyACM0 erase-flash` is destructive — only use it
deliberately, with a plan to restore credentials/configuration afterward.

## Monitor

```bash
idf.py -p /dev/ttyACM0 monitor
```

Exit with `Ctrl+]` so the serial port is released cleanly.

## Upload/register firmware with the backend

The device itself registers automatically over MQTT on first boot
(`/pub/registration`) — nothing to run for that. What you do need to run
manually is `upload.py`, which uploads the **firmware binary** to the
`mate-things` backend so it exists as a dispatchable OTA target:

1. Copy `upload.config.json.example` to `upload.config.json` (gitignored,
   never commit it) and fill in the backend URL and an admin
   username/password:
   ```json
   {
     "url": "http://<backend-host>:8080/api",
     "username": "admin",
     "password": "<password>",
     "filename": "build/mate-espidf-base.bin",
     "node_class_name": "base_node"
   }
   ```
2. After building, run:
   ```bash
   python3 upload.py [path/to/upload.config.json]
   ```
   (defaults to `upload.config.json` in the repo root if omitted).

`firmware_name` is derived automatically as `<project_name>_<version>`
(from the root `CMakeLists.txt`'s `project(...)` and `version.txt`) —
the same name the device reports at MQTT registration — so this is never
hand-typed. The script logs in, resolves the node class, and either
creates a new firmware row or replaces the binary in place if a firmware
with that name already exists, so re-running it after every rebuild just
updates the same entry. It prints the firmware's id, size, checksum, and a
freshly issued presigned download URL, ready to use in an OTA dispatch
request (`POST /v1/nodes/by-device/{device_id}/ota`).

## VSCode setup

### Clangd

1. Install the `clangd` extension.
2. Go to `Settings` > `Clangd: Arguments` and add:
   - Linux:
     ```text
     --query-driver=**/xtensa-esp-elf/**/bin/xtensa-esp*-elf-gcc,**/xtensa-esp-elf/**/bin/xtensa-esp*-elf-g++,**/riscv32-esp-elf/**/bin/riscv32-esp-elf-gcc,**/riscv32-esp-elf/**/bin/riscv32-esp-elf-g++
     ```
   - Windows:
     ```text
     --query-driver=C:/Espressif/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin/xtensa-esp32-elf-gcc.exe,C:/Espressif/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin/xtensa-esp32s3-elf-gcc.exe,C:/Espressif/tools/riscv32-esp-elf/*/riscv32-esp-elf/bin/riscv32-esp-elf-gcc.exe
     ```
3. After opening a `.c` or `.h` file, you may be prompted to install `clangd` on your machine — accept it.
4. Right-click a `.c` or `.h` file and go to `Format Document With...` > `Configure Default Formatter...` > `clangd`.
5. Go to `Settings` > `Editor: Format On Save` and enable it, so files are formatted automatically on save.

### ESP-IDF extension

Install the `ESP-IDF` extension — it helps a lot if you're not comfortable
working from the command line.
