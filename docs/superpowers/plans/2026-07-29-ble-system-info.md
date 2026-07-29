# BLE System Info + Preloaded Config Schema Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a dedicated BLE "System Info" GATT service exposing project/chip info and the preloaded-config key/type schema, backed by a new `system_info` application usecase that replaces direct use of the `dom_contracts_system_info_t` domain contract in `settings` and `messaging_callbacks`.

**Architecture:** Domain layer gains a compiled-in preloaded-config schema (X-macro-generated). A new `system_info` usecase wraps the existing `dom_contracts_system_info_t` contract plus the schema. A new BLE handler (mirroring `presentation/ble/handler/settings`'s shape) exposes both over a new GATT service (0x0004). Settings' existing snapshot drops its now-redundant `project`/`chip` fields.

**Tech Stack:** ESP-IDF v6.0.2 C, NimBLE, cJSON.

## Global Constraints

- No unit-test suite exists for this firmware. Verification is `idf.py build` (after `idf.py reconfigure` for any task that adds new source files — CMake's `file(GLOB_RECURSE ...)` snapshots at configure time) plus hardware-in-loop BLE checks against the real ESP32-C3 on `/dev/ttyACM0`, IDF env via `source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"`.
- Module lifecycle convention: `_new` (validate cfg + calloc + assign, no side effects) → `_init` (idempotent, does the wiring) → `_deinit` (idempotent, symmetric teardown) → `_delete` (calls `_deinit` then frees). Modules with no side effects (like `settings`, and the new `system_info` usecase) use only `_new`/`_delete`.
- Logging tag convention: `#define BASE_TAG "<module/path>"` + per-function `const char* tag = BASE_TAG "/<fn>";`.
- Every new struct-owned buffer used inside a NimBLE access callback must be struct-owned, never stack-local (prior crash precedent: stack protection faults on `nimble_host`/`sys_evt` tasks — see `docs/agent_test/v1.0.0-dev.1/scenario/09-known-gaps-summary.md` bugs #6/#7).
- `validate_cfg` is always extracted to its own `impl_utils.c`/`utils.c` helper, never inlined in `_new`.
- Vendor BLE UUID base: `4d415445-SSSS-4700-CCCC-000000000000`, built via `presentation/ble/gatt/uuid.c`'s `PRES_BLE_GATT_UUID128(svc_hi, svc_lo, chr_hi, chr_lo)` macro.
- This is sub-project 1 of a larger 3-part effort; do not implement the MQTT config topic, backend tables, or upload-tooling changes — those are separate specs/plans.

---

### Task 1: Preloaded config schema in the domain layer

**Files:**
- Modify: `main/include/domain/models/preloaded.h`
- Modify: `main/src/composition/main/preloaded.c`

**Interfaces:**
- Produces: `dom_models_preloaded_value_type_t` enum (`DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING`, `_UINT32`, `_BOOL`), `dom_models_preloaded_schema_entry_t { const char* key; dom_models_preloaded_value_type_t type; }`, `DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT` (7), `extern const dom_models_preloaded_schema_entry_t dom_models_preloaded_schema[DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT];` — later tasks read this array to build the BLE schema characteristic.

- [ ] **Step 1: Add the value-type enum and schema X-macro to `preloaded.h`**

Open `main/include/domain/models/preloaded.h`. Keep every existing `#define DOMAIN_MODELS_PRELOADED_*_KEY "..."` line exactly as-is (lines 11-17). Immediately after them (before the `dom_models_preloaded_t` struct), insert:

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

The full file (existing struct/extern unchanged below the insertion) should now read:

```c
#ifndef DOMAIN_MODELS_PRELOADED_H
#define DOMAIN_MODELS_PRELOADED_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DOMAIN_MODELS_PRELOADED_MQTT_PROTO_KEY                   "mqtt_proto"
#define DOMAIN_MODELS_PRELOADED_MQTT_HOST_KEY                    "mqtt_host"
#define DOMAIN_MODELS_PRELOADED_MQTT_PORT_KEY                    "mqtt_port"
#define DOMAIN_MODELS_PRELOADED_MQTT_USER_KEY                    "mqtt_user"
#define DOMAIN_MODELS_PRELOADED_MQTT_PASS_KEY                    "mqtt_pass"
#define DOMAIN_MODELS_PRELOADED_SYSTEM_RESTART_AFTER_MS_KEY      "sys_rst_aft_ms"
#define DOMAIN_MODELS_PRELOADED_WIFI_STA_TRY_CONNECT_ON_INIT_KEY "wifi_try_init"

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

typedef struct {
    uint64_t device_id;
    char*    device_id_str;
    char*    mqtt_proto;
    char*    mqtt_host;
    char*    mqtt_port;
    char*    mqtt_user;
    char*    mqtt_pass;
    uint32_t system_restart_after_ms;
    bool     wifi_sta_try_connect_on_init;
} dom_models_preloaded_t;

extern dom_models_preloaded_t dom_models_preloaded_data;

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_PRELOADED_H */
```

- [ ] **Step 2: Define the schema array's storage in `composition/main/preloaded.c`**

Open `main/src/composition/main/preloaded.c`. Find line 28 (`dom_models_preloaded_t dom_models_preloaded_data;`). Immediately after it, add:

```c
#define X(cb_name, cb_key, cb_type) {cb_key, cb_type},
const dom_models_preloaded_schema_entry_t dom_models_preloaded_schema[DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT] = {
    DOMAIN_MODELS_PRELOADED_SCHEMA(X)
};
#undef X
```

- [ ] **Step 3: Build**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py build
```

Expected: build succeeds (no new source files this task, only edits to existing files).

- [ ] **Step 4: Commit**

```bash
git add main/include/domain/models/preloaded.h main/src/composition/main/preloaded.c
git commit -m "feat: add preloaded config key/type schema"
```

---

### Task 2: `system_info` domain usecase + application impl

**Files:**
- Create: `main/include/domain/usecases/internal/system_info.h`
- Create: `main/include/application/internal/system_info/impl_types.h`
- Create: `main/include/application/internal/system_info/impl_utils.h`
- Create: `main/include/application/internal/system_info/impl.h`
- Create: `main/src/application/internal/system_info/impl_utils.c`
- Create: `main/src/application/internal/system_info/impl.c`

**Interfaces:**
- Consumes: `dom_contracts_system_info_t` (`main/include/domain/contracts/system/info.h`, unchanged — `get_project_info`, `get_chip_info`), `dom_models_preloaded_schema`/`DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT` from Task 1.
- Produces: `dom_usecases_internal_system_info_t` with `get_project_info(self, out)`, `get_chip_info(self, out)`, `get_preloaded_schema(self, out, out_count)`; `app_internal_system_info_impl_new(cfg)` / `_delete(self)`. Task 3, 4, and 7 depend on these exact names.

- [ ] **Step 1: Write the domain usecase contract**

Create `main/include/domain/usecases/internal/system_info.h`:

```c
#ifndef DOMAIN_USECASES_INTERNAL_SYSTEM_INFO_H
#define DOMAIN_USECASES_INTERNAL_SYSTEM_INFO_H

#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/preloaded.h"
#include "domain/models/system.h"

#ifdef __cplusplus
extern "C" {
#endif

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
        dom_usecases_internal_system_info_t*        self,
        const dom_models_preloaded_schema_entry_t** out,
        size_t*                                     out_count
    );
};

static inline dom_usecases_internal_system_info_t* dom_usecases_internal_system_info_new(void* ctx) {
    dom_usecases_internal_system_info_t* self = (dom_usecases_internal_system_info_t*)calloc(1, sizeof(dom_usecases_internal_system_info_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_system_info_delete(dom_usecases_internal_system_info_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_SYSTEM_INFO_H */
```

- [ ] **Step 2: Write the application impl's types header**

Create `main/include/application/internal/system_info/impl_types.h`:

```c
#ifndef APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_TYPES_H
#define APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_TYPES_H

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/system/info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dom_contracts_logger_leveled_t* logger;
    dom_contracts_system_info_t*    system_info;
} app_internal_system_info_impl_cfg_t;

typedef struct {
    app_internal_system_info_impl_cfg_t cfg;
} app_internal_system_info_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_TYPES_H */
```

- [ ] **Step 3: Write the impl_utils header + source**

Create `main/include/application/internal/system_info/impl_utils.h`:

```c
#ifndef APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_UTILS_H
#define APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_UTILS_H

#include "application/internal/system_info/impl_types.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_system_info_impl_validate_cfg(const app_internal_system_info_impl_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_UTILS_H */
```

Create `main/src/application/internal/system_info/impl_utils.c`:

```c
#include "application/internal/system_info/impl_utils.h"

static bool has_system_info_functions(dom_contracts_system_info_t* system_info);

dom_models_error_t app_internal_system_info_impl_validate_cfg(const app_internal_system_info_impl_cfg_t* cfg) {
    if (!cfg ||
        !cfg->logger ||
        !cfg->logger->error ||
        !cfg->logger->info ||
        !has_system_info_functions(cfg->system_info)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static bool has_system_info_functions(dom_contracts_system_info_t* system_info) {
    return system_info &&
           system_info->get_project_info &&
           system_info->get_chip_info;
}
```

- [ ] **Step 4: Write the impl header**

Create `main/include/application/internal/system_info/impl.h`:

```c
#ifndef APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_H
#define APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_H

#include "application/internal/system_info/impl_types.h"
#include "domain/usecases/internal/system_info.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_system_info_t* app_internal_system_info_impl_new(const app_internal_system_info_impl_cfg_t* cfg);

void app_internal_system_info_impl_delete(dom_usecases_internal_system_info_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_H */
```

- [ ] **Step 5: Write the impl source**

Create `main/src/application/internal/system_info/impl.c`:

```c
#include "application/internal/system_info/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/system_info/impl_types.h"
#include "application/internal/system_info/impl_utils.h"
#include "domain/models/error.h"
#include "domain/usecases/internal/system_info.h"

#define BASE_TAG "internal_system_info"

/* Helper Function Prototypes */

static dom_models_error_t get_ctx(
    dom_usecases_internal_system_info_t*  self,
    app_internal_system_info_impl_ctx_t** out
);

/* Contract Function Prototypes */

static dom_models_error_t get_project_info_impl(
    dom_usecases_internal_system_info_t* self,
    dom_models_system_project_info_t*    out
);
static dom_models_error_t get_chip_info_impl(
    dom_usecases_internal_system_info_t* self,
    dom_models_system_chip_info_t*       out
);
static dom_models_error_t get_preloaded_schema_impl(
    dom_usecases_internal_system_info_t*        self,
    const dom_models_preloaded_schema_entry_t** out,
    size_t*                                     out_count
);

/* Constructor and Destructor */

dom_usecases_internal_system_info_t* app_internal_system_info_impl_new(const app_internal_system_info_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    dom_models_error_t err = app_internal_system_info_impl_validate_cfg(cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_system_info_impl_ctx_t* ctx = (app_internal_system_info_impl_ctx_t*)calloc(1, sizeof(app_internal_system_info_impl_ctx_t));
    if (!ctx) {
        cfg->logger->error(cfg->logger, tag, "Failed to allocate System Info context: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_system_info_impl_cfg_t));

    dom_usecases_internal_system_info_t* self = dom_usecases_internal_system_info_new(ctx);
    if (!self) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to allocate System Info usecase: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        free(ctx);
        return NULL;
    }

    self->get_project_info    = get_project_info_impl;
    self->get_chip_info       = get_chip_info_impl;
    self->get_preloaded_schema = get_preloaded_schema_impl;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "System Info created successfully");

    return self;
}

void app_internal_system_info_impl_delete(dom_usecases_internal_system_info_t* self) {
    const char* tag = BASE_TAG "/delete";

    if (!self) {
        return;
    }

    app_internal_system_info_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "System Info deleted successfully");
        free(ctx);
    }

    dom_usecases_internal_system_info_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t get_project_info_impl(
    dom_usecases_internal_system_info_t* self,
    dom_models_system_project_info_t*    out
) {
    const char* tag = BASE_TAG "/get_project_info";

    app_internal_system_info_impl_ctx_t* ctx = NULL;
    dom_models_error_t                   err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing project info output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.system_info->get_project_info(ctx->cfg.system_info, out);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load project info: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Project info retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_chip_info_impl(
    dom_usecases_internal_system_info_t* self,
    dom_models_system_chip_info_t*       out
) {
    const char* tag = BASE_TAG "/get_chip_info";

    app_internal_system_info_impl_ctx_t* ctx = NULL;
    dom_models_error_t                   err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing chip info output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.system_info->get_chip_info(ctx->cfg.system_info, out);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load chip info: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Chip info retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_preloaded_schema_impl(
    dom_usecases_internal_system_info_t*        self,
    const dom_models_preloaded_schema_entry_t** out,
    size_t*                                     out_count
) {
    const char* tag = BASE_TAG "/get_preloaded_schema";

    app_internal_system_info_impl_ctx_t* ctx = NULL;
    dom_models_error_t                   err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out || !out_count) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing preloaded schema output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    *out       = dom_models_preloaded_schema;
    *out_count = DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Preloaded config schema retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static dom_models_error_t get_ctx(
    dom_usecases_internal_system_info_t*  self,
    app_internal_system_info_impl_ctx_t** out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = self->ctx;

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 6: Reconfigure and build**

New source files require a CMake reconfigure (`file(GLOB_RECURSE ...)` is snapshotted at configure time):

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure
idf.py build
```

Expected: build succeeds. This module isn't wired into composition yet (Task 7), so nothing new runs, but it must compile standalone.

- [ ] **Step 7: Commit**

```bash
git add main/include/domain/usecases/internal/system_info.h main/include/application/internal/system_info main/src/application/internal/system_info
git commit -m "feat: add system_info usecase wrapping the system info contract"
```

---

### Task 3: Drop `project`/`chip` from the settings snapshot

**Files:**
- Modify: `main/include/domain/usecases/internal/settings.h`
- Modify: `main/include/application/internal/settings/impl_types.h`
- Modify: `main/src/application/internal/settings/impl_utils.c`
- Modify: `main/src/presentation/ble/handler/settings/dto.c`

**Interfaces:**
- Consumes: nothing new from Task 1/2 (this task only removes dependencies).
- Produces: `dom_usecases_internal_settings_snapshot_t` without `project`/`chip` fields; `app_internal_settings_impl_cfg_t` without `system_info`. Task 7's `application.c` wiring must not set `.system_info` on `settings_cfg` anymore.

- [ ] **Step 1: Remove `project`/`chip` from the snapshot struct and drop the now-unused include**

In `main/include/domain/usecases/internal/settings.h`, remove these two lines from `dom_usecases_internal_settings_snapshot_t`:

```c
    dom_models_system_project_info_t project;
    dom_models_system_chip_info_t    chip;
```

Remove the now-unused `#include "domain/models/system.h"` from the top of the file (nothing else in the file references `dom_models_system_*`).

- [ ] **Step 2: Drop `system_info` from the settings cfg**

In `main/include/application/internal/settings/impl_types.h`, remove the line:

```c
    dom_contracts_system_info_t*          system_info;
```

Remove the now-unused `#include "domain/contracts/system/info.h"`.

- [ ] **Step 3: Drop the project/chip loading + `system_info` validation from `impl_utils.c`**

In `main/src/application/internal/settings/impl_utils.c`:

Remove this block from `app_internal_settings_impl_load_snapshot` (the two calls right before the function's final `return DOMAIN_MODELS_ERROR_OK;`):

```c
    err = ctx->cfg.system_info->get_project_info(ctx->cfg.system_info, &out->project);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.system_info->get_chip_info(ctx->cfg.system_info, &out->chip);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

```

Remove `!has_system_info_functions(cfg->system_info) ||` from `app_internal_settings_impl_validate_cfg`'s condition, remove the now-unused `static bool has_system_info_functions(...)` function and its prototype, and remove the now-unused `#include "domain/contracts/system/info.h"`.

- [ ] **Step 4: Drop `project`/`chip` from the settings BLE snapshot JSON**

In `main/src/presentation/ble/handler/settings/dto.c`'s `pres_ble_handler_settings_dto_encode_snapshot`, remove:

```c
    cJSON* project = cJSON_AddObjectToObject(root, "project");
    if (project) {
        cJSON_AddStringToObject(project, "project_name", snapshot->project.project_name);
        cJSON_AddStringToObject(project, "project_version", snapshot->project.project_version);
        cJSON_AddStringToObject(project, "name", snapshot->project.name);
        cJSON_AddStringToObject(project, "type", snapshot->project.type);
        cJSON_AddStringToObject(project, "firmware_version", snapshot->project.firmware_version);
    }

    cJSON* chip = cJSON_AddObjectToObject(root, "chip");
    if (chip) {
        cJSON_AddStringToObject(chip, "hardware_mac", snapshot->chip.hardware_mac);
        cJSON_AddStringToObject(chip, "model", snapshot->chip.model);
        cJSON_AddNumberToObject(chip, "revision", snapshot->chip.revision);
        cJSON_AddNumberToObject(chip, "cores", snapshot->chip.cores);
    }

```

The function now ends with the `system_restart_after_ms` field followed directly by `bool ok = cJSON_PrintPreallocated(...)`.

- [ ] **Step 5: Build**

Note: this task alone will NOT compile standalone, since `composition/main/application.c` still sets `.system_info = launcher->infrastructure.system_info,` on `settings_cfg` (a field that no longer exists after Step 2) — that line is only removed in Task 7. Skip the build step for this task; Task 7's build is the first point all pieces compile together. Instead, sanity-check with a narrower compile check:

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
grep -n "system_info" main/src/composition/main/application.c
```

Expected: the `settings_cfg` block's `.system_info` line is the only remaining `system_info` reference tied to `settings` (confirms Task 7 has the one remaining fix-up to make).

- [ ] **Step 6: Commit**

```bash
git add main/include/domain/usecases/internal/settings.h main/include/application/internal/settings/impl_types.h main/src/application/internal/settings/impl_utils.c main/src/presentation/ble/handler/settings/dto.c
git commit -m "refactor: drop redundant project/chip fields from settings snapshot"
```

---

### Task 4: Refactor `messaging_callbacks` to depend on the `system_info` usecase

**Files:**
- Modify: `main/include/application/internal/messaging_callbacks/impl_types.h`
- Modify: `main/src/application/internal/messaging_callbacks/impl_utils.c`

**Interfaces:**
- Consumes: `dom_usecases_internal_system_info_t` from Task 2.
- Produces: `app_internal_messaging_callbacks_impl_cfg_t.system_info` now typed `dom_usecases_internal_system_info_t*`. Task 7's `application.c` wiring must pass `launcher->application.system_info` (not `launcher->infrastructure.system_info`) here.

- [ ] **Step 1: Change the cfg field's type**

In `main/include/application/internal/messaging_callbacks/impl_types.h`, change:

```c
    dom_contracts_system_info_t*            system_info;
```

to:

```c
    dom_usecases_internal_system_info_t*    system_info;
```

Replace the include `#include "domain/contracts/system/info.h"` with `#include "domain/usecases/internal/system_info.h"`.

- [ ] **Step 2: Update the cfg-validation helper's parameter type**

In `main/src/application/internal/messaging_callbacks/impl_utils.c`, change the helper's signature and prototype from:

```c
static bool has_system_info_functions(dom_contracts_system_info_t* system_info);
```

to:

```c
static bool has_system_info_functions(dom_usecases_internal_system_info_t* system_info);
```

(both the prototype near the top of the file and the function definition at the bottom). The body is unchanged — it still checks `system_info && system_info->get_project_info && system_info->get_chip_info` (the usecase struct has the same two field names). Replace the file's `#include "domain/contracts/system/info.h"` with `#include "domain/usecases/internal/system_info.h"`.

Note: `main/src/application/internal/messaging_callbacks/impl.c` (the call sites at what were lines 178 and 185, `ctx->cfg.system_info->get_project_info(...)` / `get_chip_info(...)`) needs no changes — the usecase exposes the identical two method names with identical signatures, so the call sites are source-compatible.

- [ ] **Step 3: Commit**

```bash
git add main/include/application/internal/messaging_callbacks/impl_types.h main/src/application/internal/messaging_callbacks/impl_utils.c
git commit -m "refactor: messaging_callbacks depends on system_info usecase, not the raw contract"
```

---

### Task 5: New GATT UUIDs for the System Info service

**Files:**
- Modify: `main/include/presentation/ble/gatt/uuid.h`
- Modify: `main/src/presentation/ble/gatt/uuid.c`

**Interfaces:**
- Produces: `pres_ble_gatt_uuid_system_info_service`, `pres_ble_gatt_uuid_system_info_info_chr`, `pres_ble_gatt_uuid_system_info_config_schema_chr`. Task 6's handler consumes these exact names.

- [ ] **Step 1: Declare the UUIDs**

In `main/include/presentation/ble/gatt/uuid.h`, after the Log service block (after line 36), add:

```c
/* System info service (0x0004) */
extern const ble_uuid128_t pres_ble_gatt_uuid_system_info_service;
extern const ble_uuid128_t pres_ble_gatt_uuid_system_info_info_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_system_info_config_schema_chr;
```

- [ ] **Step 2: Define the UUIDs**

In `main/src/presentation/ble/gatt/uuid.c`, after the Log service block, add:

```c
/* System info service (0x0004) */
const ble_uuid128_t pres_ble_gatt_uuid_system_info_service            = PRES_BLE_GATT_UUID128(0x00, 0x04, 0x00, 0x00);
const ble_uuid128_t pres_ble_gatt_uuid_system_info_info_chr           = PRES_BLE_GATT_UUID128(0x00, 0x04, 0x00, 0x01);
const ble_uuid128_t pres_ble_gatt_uuid_system_info_config_schema_chr  = PRES_BLE_GATT_UUID128(0x00, 0x04, 0x00, 0x02);
```

- [ ] **Step 3: Build**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py build
```

Expected: build succeeds (no new source files, only additions to existing ones).

- [ ] **Step 4: Commit**

```bash
git add main/include/presentation/ble/gatt/uuid.h main/src/presentation/ble/gatt/uuid.c
git commit -m "feat: add GATT UUIDs for the system info BLE service"
```

---

### Task 6: BLE System Info presentation handler

**Files:**
- Create: `main/include/presentation/ble/handler/system_info/types.h`
- Create: `main/include/presentation/ble/handler/system_info/handler.h`
- Create: `main/include/presentation/ble/handler/system_info/utils.h`
- Create: `main/include/presentation/ble/handler/system_info/dto.h`
- Create: `main/src/presentation/ble/handler/system_info/utils.c`
- Create: `main/src/presentation/ble/handler/system_info/dto.c`
- Create: `main/src/presentation/ble/handler/system_info/handler.c`

**Interfaces:**
- Consumes: `dom_usecases_internal_system_info_t` (Task 2), `pres_ble_gatt_uuid_system_info_*` (Task 5), `pres_ble_gatt_registry_t` (existing), `pres_ble_gatt_util_write_read_response`/`pres_ble_gatt_util_disabled_access_callback` (existing, `presentation/ble/gatt/util.h`).
- Produces: `pres_ble_handler_system_info_t`, `pres_ble_handler_system_info_cfg_t { logger, system_info, gatt_registry }`, `pres_ble_handler_system_info_new/_delete/_init/_deinit`. Task 7's composition wiring consumes these exact names.

- [ ] **Step 1: Write `types.h`**

Create `main/include/presentation/ble/handler/system_info/types.h`:

```c
#ifndef PRESENTATION_BLE_HANDLER_SYSTEM_INFO_TYPES_H
#define PRESENTATION_BLE_HANDLER_SYSTEM_INFO_TYPES_H

#include <stdbool.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/usecases/internal/system_info.h"
#include "presentation/ble/gatt/registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_BLE_HANDLER_SYSTEM_INFO_INFO_JSON_MAX_LEN          192
#define PRES_BLE_HANDLER_SYSTEM_INFO_CONFIG_SCHEMA_JSON_MAX_LEN 256

typedef struct {
    dom_contracts_logger_leveled_t*      logger;
    dom_usecases_internal_system_info_t* system_info;
    pres_ble_gatt_registry_t*            gatt_registry;
} pres_ble_handler_system_info_cfg_t;

typedef struct pres_ble_handler_system_info_t {
    pres_ble_handler_system_info_cfg_t cfg;
    bool                                registered;
    /* Struct-owned (not stack-local) for the same reason as
       presentation/ble/handler/settings/types.h's buffers: a read access
       callback runs on the nimble_host task, whose configured stack is
       small - a stack-local buffer here previously caused a stack
       protection fault/crash on settings-data reads. */
    char info_json[PRES_BLE_HANDLER_SYSTEM_INFO_INFO_JSON_MAX_LEN];
    char config_schema_json[PRES_BLE_HANDLER_SYSTEM_INFO_CONFIG_SCHEMA_JSON_MAX_LEN];
} pres_ble_handler_system_info_t;

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SYSTEM_INFO_TYPES_H */
```

- [ ] **Step 2: Write `handler.h`**

Create `main/include/presentation/ble/handler/system_info/handler.h`:

```c
#ifndef PRESENTATION_BLE_HANDLER_SYSTEM_INFO_HANDLER_H
#define PRESENTATION_BLE_HANDLER_SYSTEM_INFO_HANDLER_H

#include "domain/models/error.h"
#include "presentation/ble/handler/system_info/types.h"

#ifdef __cplusplus
extern "C" {
#endif

pres_ble_handler_system_info_t* pres_ble_handler_system_info_new(const pres_ble_handler_system_info_cfg_t* cfg);

void pres_ble_handler_system_info_delete(pres_ble_handler_system_info_t* self);

/* Adds this feature's GATT service to cfg.gatt_registry. Must be called
   before the shared registry's register_all() (see gatt/registry.h). */
dom_models_error_t pres_ble_handler_system_info_init(pres_ble_handler_system_info_t* self);

/* NimBLE never allows a registered characteristic to be unregistered, so
   this neuters the access callbacks instead of removing anything - see
   gatt/util.h's disabled_access_callback. */
void pres_ble_handler_system_info_deinit(pres_ble_handler_system_info_t* self);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SYSTEM_INFO_HANDLER_H */
```

- [ ] **Step 3: Write `utils.h` + `utils.c`**

Create `main/include/presentation/ble/handler/system_info/utils.h`:

```c
#ifndef PRESENTATION_BLE_HANDLER_SYSTEM_INFO_UTILS_H
#define PRESENTATION_BLE_HANDLER_SYSTEM_INFO_UTILS_H

#include "domain/models/error.h"
#include "presentation/ble/handler/system_info/types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_ble_handler_system_info_validate_cfg(const pres_ble_handler_system_info_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SYSTEM_INFO_UTILS_H */
```

Create `main/src/presentation/ble/handler/system_info/utils.c`:

```c
#include "presentation/ble/handler/system_info/utils.h"

dom_models_error_t pres_ble_handler_system_info_validate_cfg(const pres_ble_handler_system_info_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->system_info || !cfg->gatt_registry) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 4: Write `dto.h` + `dto.c`**

Create `main/include/presentation/ble/handler/system_info/dto.h`:

```c
#ifndef PRESENTATION_BLE_HANDLER_SYSTEM_INFO_DTO_H
#define PRESENTATION_BLE_HANDLER_SYSTEM_INFO_DTO_H

#include <stddef.h>

#include "domain/models/preloaded.h"
#include "domain/models/system.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t pres_ble_handler_system_info_dto_encode_info(
    const dom_models_system_project_info_t* project,
    const dom_models_system_chip_info_t*    chip,
    char*                                    buf,
    size_t                                   buf_cap
);

size_t pres_ble_handler_system_info_dto_encode_config_schema(
    const dom_models_preloaded_schema_entry_t* entries,
    size_t                                      count,
    char*                                       buf,
    size_t                                      buf_cap
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SYSTEM_INFO_DTO_H */
```

Create `main/src/presentation/ble/handler/system_info/dto.c`:

```c
#include "presentation/ble/handler/system_info/dto.h"

#include <string.h>

#include "cJSON.h"

static const char* value_type_str(dom_models_preloaded_value_type_t type);

size_t pres_ble_handler_system_info_dto_encode_info(
    const dom_models_system_project_info_t* project,
    const dom_models_system_chip_info_t*    chip,
    char*                                    buf,
    size_t                                   buf_cap
) {
    if (!project || !chip || !buf || buf_cap == 0) {
        return 0;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return 0;
    }

    cJSON* project_obj = cJSON_AddObjectToObject(root, "project");
    if (project_obj) {
        cJSON_AddStringToObject(project_obj, "project_name", project->project_name);
        cJSON_AddStringToObject(project_obj, "project_version", project->project_version);
        cJSON_AddStringToObject(project_obj, "name", project->name);
        cJSON_AddStringToObject(project_obj, "type", project->type);
        cJSON_AddStringToObject(project_obj, "firmware_version", project->firmware_version);
    }

    cJSON* chip_obj = cJSON_AddObjectToObject(root, "chip");
    if (chip_obj) {
        cJSON_AddStringToObject(chip_obj, "hardware_mac", chip->hardware_mac);
        cJSON_AddStringToObject(chip_obj, "model", chip->model);
        cJSON_AddNumberToObject(chip_obj, "revision", chip->revision);
        cJSON_AddNumberToObject(chip_obj, "cores", chip->cores);
    }

    bool ok = cJSON_PrintPreallocated(root, buf, (int)buf_cap, false);
    cJSON_Delete(root);

    return ok ? strnlen(buf, buf_cap) : 0;
}

size_t pres_ble_handler_system_info_dto_encode_config_schema(
    const dom_models_preloaded_schema_entry_t* entries,
    size_t                                      count,
    char*                                       buf,
    size_t                                       buf_cap
) {
    if (!entries || !buf || buf_cap == 0) {
        return 0;
    }

    cJSON* root = cJSON_CreateArray();
    if (!root) {
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        cJSON* entry = cJSON_CreateObject();
        if (!entry) {
            cJSON_Delete(root);
            return 0;
        }

        cJSON_AddStringToObject(entry, "key", entries[i].key);
        cJSON_AddStringToObject(entry, "type", value_type_str(entries[i].type));
        cJSON_AddItemToArray(root, entry);
    }

    bool ok = cJSON_PrintPreallocated(root, buf, (int)buf_cap, false);
    cJSON_Delete(root);

    return ok ? strnlen(buf, buf_cap) : 0;
}

static const char* value_type_str(dom_models_preloaded_value_type_t type) {
    switch (type) {
        case DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING:
            return "string";
        case DOMAIN_MODELS_PRELOADED_VALUE_TYPE_UINT32:
            return "uint32";
        case DOMAIN_MODELS_PRELOADED_VALUE_TYPE_BOOL:
            return "bool";
    }
    return "unknown";
}
```

- [ ] **Step 5: Write `handler.c`**

Create `main/src/presentation/ble/handler/system_info/handler.c`:

```c
#include "presentation/ble/handler/system_info/handler.h"

#include <stdlib.h>
#include <string.h>

#include "domain/models/error.h"
#include "host/ble_att.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "presentation/ble/gatt/util.h"
#include "presentation/ble/gatt/uuid.h"
#include "presentation/ble/handler/system_info/dto.h"
#include "presentation/ble/handler/system_info/utils.h"

#define BASE_TAG "ble/handler/system_info"

/* Lifetime BLE Definitions */

static struct ble_gatt_chr_def characteristic_defs[3];
static struct ble_gatt_svc_def service_defs[2];

/* Access Callback Function Prototypes */

static int info_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int config_schema_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);

/* Constructors and Destructors */

pres_ble_handler_system_info_t* pres_ble_handler_system_info_new(const pres_ble_handler_system_info_cfg_t* cfg) {
    if (pres_ble_handler_system_info_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    pres_ble_handler_system_info_t* self = (pres_ble_handler_system_info_t*)calloc(1, sizeof(pres_ble_handler_system_info_t));
    if (!self) {
        return NULL;
    }

    memcpy(&self->cfg, cfg, sizeof(pres_ble_handler_system_info_cfg_t));

    return self;
}

void pres_ble_handler_system_info_delete(pres_ble_handler_system_info_t* self) {
    if (!self) {
        return;
    }

    pres_ble_handler_system_info_deinit(self);
    free(self);
}

dom_models_error_t pres_ble_handler_system_info_init(pres_ble_handler_system_info_t* self) {
    const char* tag = BASE_TAG "/init";

    if (!self) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->registered) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    characteristic_defs[0] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_system_info_info_chr.u,
        .access_cb = info_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_READ,
    };
    characteristic_defs[1] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_system_info_config_schema_chr.u,
        .access_cb = config_schema_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_READ,
    };
    characteristic_defs[2] = (struct ble_gatt_chr_def){0};

    service_defs[0] = (struct ble_gatt_svc_def){
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &pres_ble_gatt_uuid_system_info_service.u,
        .characteristics = characteristic_defs,
    };
    service_defs[1] = (struct ble_gatt_svc_def){0};

    dom_models_error_t err = pres_ble_gatt_registry_add_service(self->cfg.gatt_registry, &service_defs[0]);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to add system info GATT service: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    self->registered = true;

    self->cfg.logger->info(self->cfg.logger, tag, "System Info BLE handler initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_ble_handler_system_info_deinit(pres_ble_handler_system_info_t* self) {
    if (!self || !self->registered) {
        return;
    }

    for (size_t i = 0; i < 2; i++) {
        characteristic_defs[i].access_cb = pres_ble_gatt_util_disabled_access_callback;
        characteristic_defs[i].arg       = NULL;
    }

    self->registered = false;
}

/* Access Callback Function Implementations */

static int info_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_system_info_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    dom_models_system_project_info_t project;
    dom_models_error_t               err = self->cfg.system_info->get_project_info(self->cfg.system_info, &project);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    dom_models_system_chip_info_t chip;
    err = self->cfg.system_info->get_chip_info(self->cfg.system_info, &chip);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t json_len = pres_ble_handler_system_info_dto_encode_info(&project, &chip, self->info_json, sizeof(self->info_json));
    if (json_len == 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return pres_ble_gatt_util_write_read_response(ctxt, self->info_json, json_len);
}

static int config_schema_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_system_info_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    const dom_models_preloaded_schema_entry_t* entries    = NULL;
    size_t                                      entry_count = 0;
    dom_models_error_t                          err         = self->cfg.system_info->get_preloaded_schema(self->cfg.system_info, &entries, &entry_count);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t json_len = pres_ble_handler_system_info_dto_encode_config_schema(entries, entry_count, self->config_schema_json, sizeof(self->config_schema_json));
    if (json_len == 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return pres_ble_gatt_util_write_read_response(ctxt, self->config_schema_json, json_len);
}
```

- [ ] **Step 6: Reconfigure and build**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure
idf.py build
```

Expected: build succeeds. Not wired into composition yet (Task 7), so this only proves the new handler compiles standalone against the registry/util/uuid APIs.

- [ ] **Step 7: Commit**

```bash
git add main/include/presentation/ble/handler/system_info main/src/presentation/ble/handler/system_info
git commit -m "feat: add BLE handler for the system info GATT service"
```

---

### Task 7: Composition wiring, build, and hardware verification

**Files:**
- Modify: `main/include/composition/main/types.h`
- Modify: `main/src/composition/main/application.c`
- Modify: `main/src/composition/main/presentation.c`
- Modify: `docs/agent_test/v1.0.0-dev.1/scenario/10-ble-gatt-services.md`

**Interfaces:**
- Consumes: `app_internal_system_info_impl_new/_delete` (Task 2), `pres_ble_handler_system_info_new/_init/_deinit/_delete` (Task 6).
- Produces: a fully wired, buildable, flashable firmware with the new BLE service live.

- [ ] **Step 1: Add the new fields to `types.h`**

In `main/include/composition/main/types.h`:

Add to the includes block:
```c
#include "domain/usecases/internal/system_info.h"
```
```c
#include "presentation/ble/handler/system_info/handler.h"
```
(keep both alphabetically placed among the existing includes, matching the file's existing ordering convention).

Add to `cmp_main_launcher_application_t`:
```c
    dom_usecases_internal_system_info_t*         system_info;
```

Add to `cmp_main_launcher_presentation_t`:
```c
    pres_ble_handler_system_info_t*  ble_system_info;
```

- [ ] **Step 2: Wire the `system_info` usecase in `application.c`**

In `main/src/composition/main/application.c`, add the include:
```c
#include "application/internal/system_info/impl.h"
```

Before the existing `app_internal_settings_impl_cfg_t settings_cfg = { ... };` block, insert:

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

In the existing `settings_cfg` block, remove the line:
```c
        .system_info          = launcher->infrastructure.system_info,
```

In the existing `messaging_callbacks_cfg` block, change:
```c
        .system_info          = launcher->infrastructure.system_info,
```
to:
```c
        .system_info          = launcher->application.system_info,
```

In `cmp_main_application_deinit`, add (after the `messaging_callbacks` teardown block, since `messaging_callbacks` depends on `system_info` and must be torn down first; `system_info` has no `_init`/`_deinit`, only `_delete`, matching `settings`):

```c
    if (launcher->application.system_info) {
        app_internal_system_info_impl_delete(launcher->application.system_info);
        launcher->application.system_info = NULL;
    }

```

Place this block right after the `messaging_callbacks` teardown block and before the `log_forwarding` teardown block.

- [ ] **Step 3: Wire the `ble_system_info` handler in `presentation.c`**

In `main/src/composition/main/presentation.c`, add the include:
```c
#include "presentation/ble/handler/system_info/handler.h"
```

After the existing `ble_wifi_manager` block (after its `pres_ble_handler_wifi_manager_init` call and error check, before the `ble_device_name`/`ble_host` block), insert:

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

In `cmp_main_presentation_deinit`, add (alongside the other BLE handler deletes, before the `ble_gatt_registry` delete since the registry must outlive every handler that added a service to it):

```c
    if (launcher->presentation.ble_system_info) {
        pres_ble_handler_system_info_delete(launcher->presentation.ble_system_info);
        launcher->presentation.ble_system_info = NULL;
    }

```

- [ ] **Step 4: Reconfigure, build, and flash**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure
idf.py build
idf.py -p /dev/ttyACM0 flash
```

Expected: build succeeds; device boots without crashing (watch `idf.py -p /dev/ttyACM0 monitor` briefly for a stack-protection fault or reset loop, matching the crash-diagnosis approach used earlier this session for bugs #6/#7).

- [ ] **Step 5: Hardware-in-loop BLE verification**

Using `bleak` (Python) from a venv against the real ESP32-C3, connect and:
1. Discover services; confirm a new service with UUID `4d415445-0004-4700-0000-000000000000` and two characteristics `4d415445-0004-4700-0001-000000000000` (info) / `4d415445-0004-4700-0002-000000000000` (config_schema) are present, both read-only.
2. Read the `info` characteristic; confirm the JSON has `project`/`chip` sub-objects matching the device's actual values (cross-check against `idf.py monitor`'s boot log or the MQTT registration payload from `docs/agent_test/v1.0.0-dev.1/scenario/03-mqtt-registration-status.md`).
3. Read the `config_schema` characteristic; confirm all 7 entries appear with the expected types: `mqtt_proto`/`mqtt_host`/`mqtt_port`/`mqtt_user`/`mqtt_pass` as `"string"`, `sys_rst_aft_ms` as `"uint32"`, `wifi_try_init` as `"bool"`.
4. Read the Settings service's `data` characteristic (UUID `...0001-0001...`); confirm its JSON no longer contains `project` or `chip` keys.

- [ ] **Step 6: Update the BLE GATT services test doc**

In `docs/agent_test/v1.0.0-dev.1/scenario/10-ble-gatt-services.md`:

Update the service table (after the existing 3-row table):
```markdown
| Service | UUID (SSSS) |
|---|---|
| Settings | `0001` |
| WiFi manager | `0002` |
| Log | `0003` |
| System info | `0004` |
```

After the existing `Log (0003): ...` line, add:
```markdown
System info (`0004`): `0001` info (R, JSON `{project,chip}`), `0002`
config_schema (R, JSON array of `{key,type}` for every preloaded config
variable).
```

At the end of the `Results — this pass` list, add:
```markdown
- [x] **BLE-12** — System info `info` read returns project/chip data
  matching the device's actual values; `config_schema` read lists all 7
  preloaded config keys with correct types; Settings `data` no longer
  includes `project`/`chip` (positive)
```

- [ ] **Step 7: Commit**

```bash
git add main/include/composition/main/types.h main/src/composition/main/application.c main/src/composition/main/presentation.c docs/agent_test/v1.0.0-dev.1/scenario/10-ble-gatt-services.md
git commit -m "feat: wire the system info BLE service into composition"
```
