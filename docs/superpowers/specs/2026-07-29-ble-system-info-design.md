# BLE System Info + Preloaded Config Schema Design

## Context

This is sub-project 1 of a larger 3-part effort (dynamic device configuration).
The other two parts — end-to-end dynamic config via MQTT + backend storage,
and upload-tooling hygiene — are separate specs that build on this one.

Today, project info (`dom_models_system_project_info_t`) and chip info
(`dom_models_system_chip_info_t`) are only reachable two ways:

1. Bundled inside the Settings BLE service's `data` characteristic JSON
   (`project`/`chip` sub-objects), read via `dom_contracts_system_info_t`
   injected directly into the `settings` usecase.
2. Consumed directly by `messaging_callbacks` (also via the raw contract) to
   build the MQTT registration payload's `firmware_name`/`device_info`.

There is no dedicated system-info usecase, and no BLE-facing way to learn
which preloaded config keys exist or what type each one is — both are
needed as a foundation for the dynamic-config work in sub-project 2.

## Goal

1. Introduce a `system_info` application usecase as the single point through
   which project info, chip info, and (new) the preloaded-config schema are
   read, replacing direct consumption of `dom_contracts_system_info_t` in
   `settings` and `messaging_callbacks`.
2. Expose project info, chip info, and the preloaded-config key/type schema
   over BLE, via a new dedicated `System Info` GATT service (0x0004).
3. Remove `project`/`chip` from the Settings service's existing snapshot
   JSON, since they become redundant with the new service (accepted
   breaking change to that payload).

## Domain layer changes

### `domain/models/preloaded.h`

The 7 existing `_KEY` string `#define`s are unchanged — they're used
directly as NVS keys in `infrastructure/repository/preloaded/nvs_impl.c`
and `composition/main/preloaded.c`.

Add a value-type enum and a schema X-macro that references the existing
`_KEY` macros by name (the preprocessor expands them when substituted as
macro arguments, so the string literal is not duplicated):

```c
typedef enum {
    DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING,
    DOMAIN_MODELS_PRELOADED_VALUE_TYPE_UINT32,
    DOMAIN_MODELS_PRELOADED_VALUE_TYPE_BOOL,
} dom_models_preloaded_value_type_t;

#define DOMAIN_MODELS_PRELOADED_SCHEMA(X)                                                                                      \
    X(MQTT_PROTO, DOMAIN_MODELS_PRELOADED_MQTT_PROTO_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                           \
    X(MQTT_HOST, DOMAIN_MODELS_PRELOADED_MQTT_HOST_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                             \
    X(MQTT_PORT, DOMAIN_MODELS_PRELOADED_MQTT_PORT_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                             \
    X(MQTT_USER, DOMAIN_MODELS_PRELOADED_MQTT_USER_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                             \
    X(MQTT_PASS, DOMAIN_MODELS_PRELOADED_MQTT_PASS_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                             \
    X(SYSTEM_RESTART_AFTER_MS, DOMAIN_MODELS_PRELOADED_SYSTEM_RESTART_AFTER_MS_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_UINT32) \
    X(WIFI_STA_TRY_CONNECT_ON_INIT, DOMAIN_MODELS_PRELOADED_WIFI_STA_TRY_CONNECT_ON_INIT_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_BOOL)

typedef struct {
    const char*                       key;
    dom_models_preloaded_value_type_t type;
} dom_models_preloaded_schema_entry_t;

#define DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT 7

extern const dom_models_preloaded_schema_entry_t dom_models_preloaded_schema[DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT];
```

`device_id`/`device_id_str` are excluded from the schema — they are
identity, not configurable settings (`dom_usecases_internal_settings_preloaded_update_t`
already excludes them from updates for the same reason).

### `composition/main/preloaded.c`

Add the schema array's storage (mirrors how `dom_models_preloaded_data`'s
storage lives here rather than in `domain/models`), generated from the
X-macro:

```c
#define X(cb_name, cb_key, cb_type) {cb_key, cb_type},
const dom_models_preloaded_schema_entry_t dom_models_preloaded_schema[DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT] = {
    DOMAIN_MODELS_PRELOADED_SCHEMA(X)
};
#undef X
```

## Application layer changes

### New: `domain/usecases/internal/system_info.h`

```c
typedef struct dom_usecases_internal_system_info_t dom_usecases_internal_system_info_t;

struct dom_usecases_internal_system_info_t {
    void* ctx;
    dom_models_error_t (*get_project_info)(
        dom_usecases_internal_system_info_t* self,
        dom_models_system_project_info_t*    out
    );
    dom_models_error_t (*get_chip_info)(
        dom_usecases_internal_system_info_t* self,
        dom_models_system_chip_info_t*       out
    );
    dom_models_error_t (*get_preloaded_schema)(
        dom_usecases_internal_system_info_t* self,
        const dom_models_preloaded_schema_entry_t** out,
        size_t*                                     out_count
    );
};
```

`get_preloaded_schema` returns a pointer to the compiled-in
`dom_models_preloaded_schema` array plus its count — no copying needed,
the data is `const` and lives for the process lifetime.

### New: `application/internal/system_info/{impl.h,impl_types.h,impl.c}`

Same shape as `application/internal/settings`: `_new`/`_delete` only, no
`_init`/`_deinit` (no side effects — pure pass-through over the injected
`dom_contracts_system_info_t*`). Cfg struct: `{ logger, system_info }`
(the domain contract). `validate_cfg` extracted to `impl_utils.c` per the
established convention.

### Refactor: `application/internal/settings`

- Drop `system_info` from `app_internal_settings_impl_cfg_t` — once
  `project`/`chip` are removed from the settings snapshot (see below),
  settings has no remaining use for it.
- `dom_usecases_internal_settings_snapshot_t` drops its `project` and
  `chip` fields.
- `get_snapshot`'s implementation in `impl_utils.c` drops the
  `system_info->get_project_info`/`get_chip_info` calls and the
  `has_system_info_functions` cfg-validation check.

### Refactor: `application/internal/messaging_callbacks`

- `app_internal_messaging_callbacks_impl_cfg_t`'s `system_info` field
  changes type from `dom_contracts_system_info_t*` to
  `dom_usecases_internal_system_info_t*`.
- `has_system_info_functions` in `impl_utils.c` checks the usecase's
  `get_project_info`/`get_chip_info` instead of the raw contract's.
- Call sites (`impl.c`) are unchanged in behavior — same two calls, now
  through the usecase instead of the contract.

## Presentation layer changes

### New GATT UUIDs — `presentation/ble/gatt/uuid.h` / `uuid.c`

```c
/* System info service (0x0004) */
extern const ble_uuid128_t pres_ble_gatt_uuid_system_info_service;
extern const ble_uuid128_t pres_ble_gatt_uuid_system_info_info_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_system_info_config_schema_chr;
```
UUID bytes via the existing `PRES_BLE_GATT_UUID128(0x00, 0x04, 0x00, 0x0N)` macro.

### New: `presentation/ble/handler/system_info/{types.h,handler.h,handler.c,dto.h,dto.c,utils.h,utils.c}`

Mirrors `presentation/ble/handler/settings`'s structure exactly:

- `pres_ble_handler_system_info_cfg_t`: `{ logger, system_info (usecase), gatt_registry }`.
- `pres_ble_handler_system_info_t`: struct-owned JSON buffers for the two
  read characteristics (`info_json[192]`, `config_schema_json[256]` —
  sized generously above worst-case encoded length, same rationale as the
  crash-precedent comment on `settings`/`wifi_manager`'s buffers).
- `_new`/`_delete`/`_init`/`_deinit` lifecycle (matches `settings`: `_init`
  registers the 1-service/2-characteristic GATT def, `_delete` calls
  `_deinit` first).
- Both characteristics are `BLE_GATT_CHR_F_READ` only (no write, no notify
  — this is static/near-static descriptive data).

**`info` characteristic** — JSON:
```json
{
  "project": {
    "project_name": "...", "project_version": "...",
    "name": "...", "type": "...", "firmware_version": "..."
  },
  "chip": {
    "hardware_mac": "...", "model": "...", "revision": 0, "cores": 0
  }
}
```

**`config_schema` characteristic** — JSON array, value types serialized as
their lowercase name (`"string"`, `"uint32"`, `"bool"`):
```json
[
  {"key": "mqtt_proto", "type": "string"},
  {"key": "mqtt_host", "type": "string"},
  {"key": "mqtt_port", "type": "string"},
  {"key": "mqtt_user", "type": "string"},
  {"key": "mqtt_pass", "type": "string"},
  {"key": "sys_rst_aft_ms", "type": "uint32"},
  {"key": "wifi_try_init", "type": "bool"}
]
```
(Only the key names and types are exposed — never values — so including
`mqtt_pass`'s key here carries no sensitivity; the existing "no BLE
pairing" caveat about the settings snapshot doesn't apply since no secret
value is present.)

### `presentation/ble/handler/settings`

- `dto.c`'s `pres_ble_handler_settings_dto_encode_snapshot` drops the
  `project`/`chip` sub-objects from its JSON output. Accepted breaking
  change to the wire payload (per confirmed decision).

## Composition wiring

### `composition/main/types.h`

- `cmp_main_launcher_application_t` gains `dom_usecases_internal_system_info_t* system_info;`.
- `cmp_main_launcher_presentation_t` gains `pres_ble_handler_system_info_t* ble_system_info;`.

### `composition/main/application.c`

Construct `system_info` usecase first (no `_init` call needed — no side
effects), before `settings` and `messaging_callbacks`, since both are
refactored to reference it:

```c
app_internal_system_info_impl_cfg_t system_info_cfg = {
    .logger      = launcher->infrastructure.logger,
    .system_info = launcher->infrastructure.system_info,
};
launcher->application.system_info = app_internal_system_info_impl_new(&system_info_cfg);
if (!launcher->application.system_info) {
    return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
}
```

`settings_cfg` drops its `.system_info = ...` line entirely.
`messaging_callbacks_cfg`'s `.system_info = ...` now points at
`launcher->application.system_info` instead of
`launcher->infrastructure.system_info`.

`cmp_main_application_deinit`: `system_info` has no `_deinit` (matches
`settings`), just `app_internal_system_info_impl_delete`, freed after
`messaging_callbacks` (which depends on it) and `settings`, alongside
`ota`/`settings` at the end.

### `composition/main/presentation.c`

Construct+init `ble_system_info` alongside the other BLE handlers
(`ble_settings`, `ble_wifi_manager`, `ble_log`), before `pres_ble_host_start`:

```c
pres_ble_handler_system_info_cfg_t ble_system_info_cfg = {
    .logger        = launcher->infrastructure.logger,
    .system_info   = launcher->application.system_info,
    .gatt_registry = launcher->presentation.ble_gatt_registry,
};
launcher->presentation.ble_system_info = pres_ble_handler_system_info_new(&ble_system_info_cfg);
if (!launcher->presentation.ble_system_info) {
    return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
}
err = pres_ble_handler_system_info_init(launcher->presentation.ble_system_info);
if (err != DOMAIN_MODELS_ERROR_OK) {
    return err;
}
```

`cmp_main_presentation_deinit`: `pres_ble_handler_system_info_delete`
alongside the other BLE handler deletes.

## Out of scope

- Any write/update path for system info or the config schema (both are
  read-only, descriptive data).
- Sub-project 2's MQTT `/sub/{device_id}/config` topic and backend
  storage — this spec only produces the on-device schema the next
  sub-project will consume.
- Sub-project 3's upload-tooling changes.

## Verification

No unit-test suite exists for this firmware (per project convention).
Verification is `idf.py build` plus hardware-in-loop checks against the
real ESP32-C3 over BLE (via `bleak`, same approach as prior BLE work this
session):

1. Build and flash; confirm no crash on boot (GATT service registration,
   advertising).
2. Connect a BLE central, discover services; confirm the new System Info
   service (0x0004) and its two characteristics are present.
3. Read `info`; confirm the JSON matches the device's actual project/chip
   values (cross-check against `idf.py monitor` boot log or the MQTT
   registration payload).
4. Read `config_schema`; confirm all 7 keys appear with the expected
   types.
5. Read the Settings service's `data` characteristic; confirm `project`/
   `chip` are no longer present in that JSON.
