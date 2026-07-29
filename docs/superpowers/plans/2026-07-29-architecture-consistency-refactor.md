# Architecture Consistency Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring every module in `mate-espidf-base` into line with one canonical pattern per architectural concern (lifecycle, log-forwarding layering, DTO placement, buffer ownership, logging tags, cfg validation, error-logging responsibility, callback-overflow handling), using the pattern that already appears most consistently in the codebase as the template — no behavior change visible to the backend or over the wire.

**Architecture:** ESP-IDF C firmware, layered `domain / application / presentation / infrastructure / composition`, mirrored under `main/include` and `main/src`. This plan retrofits the MQTT transport and a few outlier modules to match conventions the BLE transport (and, for lifecycle, `wifi_manager`) already established correctly.

**Tech Stack:** ESP-IDF v6.0.2, C11, cJSON, NimBLE, esp-mqtt. Build via `idf.py build` (no unit-test framework in this repo).

## Global Constraints

- No change to any wire payload shape: MQTT topic strings, JSON field names, BLE GATT UUIDs/characteristic semantics stay byte-identical. The backend and any BLE central must observe zero difference.
- No new features, no new transports, no domain-layer data model changes.
- Every module's public function naming keeps its existing prefix family (`pres_ble_*`, `pres_mqtt_*`, `app_internal_*`, `dom_*`, `cmp_main_*`, `inf_*`) — only internal structure changes.
- `idf.py build` must be clean (no new warnings) after every task before moving to the next.
- Full command to build (run from repo root): `source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh && cd /home/dodol/Repositories/mate/mate-espidf-base && idf.py build`
- Design reference: `docs/superpowers/specs/2026-07-29-architecture-consistency-refactor-design.md`.
- Hardware-in-loop verification (only needed at the phase checkpoints called out below, not every task) uses the real ESP32-C3 on `/dev/ttyACM0` and the scenarios under `docs/agent_test/v1.0.0-dev.1/`.

---

## Phase 1 — Logger callback-overflow contract change

### Task 1: `dom_contracts_logger_leveled_t.add_callback` returns `dom_models_error_t`

**Files:**
- Modify: `main/include/domain/contracts/logger/leveled.h:41-45`
- Modify: `main/src/infrastructure/logger/leveled/stdio_impl.c:47-51,188-210`
- Modify: `main/src/presentation/ble/handler/log/handler.c:122` (temporary — Phase 3 replaces this call site entirely)
- Modify: `main/src/application/internal/messaging_callbacks/impl.c:94` (temporary — Phase 3 replaces this call site entirely)

**Interfaces:**
- Produces: `dom_models_error_t (*add_callback)(dom_contracts_logger_leveled_t* self, void* cb_ctx, dom_contracts_logger_leveled_cb cb_func)` — callers now check the return value; `DOMAIN_MODELS_ERROR_BAD_STATE` means the fixed-size callback array (`cfg.cb_max_cnt`) is full, `DOMAIN_MODELS_ERROR_BAD_ARGUMENT` means `self`/`self->ctx`/`cb_func` was null.

- [ ] **Step 1: Change the contract signature**

In `main/include/domain/contracts/logger/leveled.h`, replace:
```c
    void (*add_callback)(
        dom_contracts_logger_leveled_t* self,
        void*                           cb_ctx,
        dom_contracts_logger_leveled_cb cb_func
    );
```
with:
```c
    dom_models_error_t (*add_callback)(
        dom_contracts_logger_leveled_t* self,
        void*                           cb_ctx,
        dom_contracts_logger_leveled_cb cb_func
    );
```
This header has no `#include "domain/models/error.h"` today — add it near the top (after `#include <stdlib.h>`):
```c
#include "domain/models/error.h"
```

- [ ] **Step 2: Update the stdio implementation's prototype and body**

In `main/src/infrastructure/logger/leveled/stdio_impl.c`, change the prototype (line 47) from `static void add_callback_impl(` to `static dom_models_error_t add_callback_impl(`.

Replace the implementation (lines 188-210):
```c
static void add_callback_impl(
    dom_contracts_logger_leveled_t* self,
    void*                           cb_ctx,
    dom_contracts_logger_leveled_cb cb_func
) {
    if (!self || !self->ctx) {
        return;
    }

    inf_logger_leveled_stdio_impl_ctx_t* ctx = self->ctx;

    if (ctx->cb_idx >= ctx->cfg.cb_max_cnt) {
        return;
    }

    if (!ctx->cb_funcs || !ctx->cb_ctxs || !cb_func) {
        return;
    }

    ctx->cb_funcs[ctx->cb_idx] = cb_func;
    ctx->cb_ctxs[ctx->cb_idx]  = cb_ctx;
    ctx->cb_idx += 1;
}
```
with:
```c
static dom_models_error_t add_callback_impl(
    dom_contracts_logger_leveled_t* self,
    void*                           cb_ctx,
    dom_contracts_logger_leveled_cb cb_func
) {
    if (!self || !self->ctx || !cb_func) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_logger_leveled_stdio_impl_ctx_t* ctx = self->ctx;

    if (!ctx->cb_funcs || !ctx->cb_ctxs) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    if (ctx->cb_idx >= ctx->cfg.cb_max_cnt) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    ctx->cb_funcs[ctx->cb_idx] = cb_func;
    ctx->cb_ctxs[ctx->cb_idx]  = cb_ctx;
    ctx->cb_idx += 1;

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 3: Temporarily patch the two existing call sites so the build stays green**

These two call sites are fully replaced in Phase 3 (Task 5), so this is a minimal, throwaway fix — just check-and-log, don't restructure anything else yet.

In `main/src/presentation/ble/handler/log/handler.c`, in `pres_ble_handler_log_init`, replace:
```c
    self->cfg.logger->add_callback(self->cfg.logger, self, on_log_message);
    self->logger_cb_subscribed = true;
```
with:
```c
    err = self->cfg.logger->add_callback(self->cfg.logger, self, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to subscribe to logger callbacks: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }
    self->logger_cb_subscribed = true;
```
(There is already an `err` variable in scope in that function from the `pres_ble_host_add_gap_event_callback` call a few lines above — reuse it, don't redeclare.)

In `main/src/application/internal/messaging_callbacks/impl.c`, in `app_internal_messaging_callbacks_impl_new`, replace:
```c
    ctx->cfg.logger->add_callback(ctx->cfg.logger, ctx, on_log_message);

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks created successfully");
```
with:
```c
    err = ctx->cfg.logger->add_callback(ctx->cfg.logger, ctx, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to logger callbacks: %s (%d)", dom_models_error_str(err), (int)err);
        dom_usecases_internal_messaging_callbacks_delete(self);
        free(ctx);
        return NULL;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks created successfully");
```
(`err` is already declared earlier in this function from the `validate_cfg` call — reuse it.)

- [ ] **Step 4: Build**

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py build
```
Expected: clean build, no warnings about implicit conversion or unused return value.

- [ ] **Step 5: Commit**

```bash
git add main/include/domain/contracts/logger/leveled.h main/src/infrastructure/logger/leveled/stdio_impl.c main/src/presentation/ble/handler/log/handler.c main/src/application/internal/messaging_callbacks/impl.c
git commit -m "refactor: logger add_callback returns dom_models_error_t on overflow instead of silently dropping"
```

---

## Phase 2 — Lifecycle shape normalization (`_new`/`_init`/`_deinit`/`_delete`)

`wifi_manager` already implements the target shape correctly (`app_internal_wifi_manager_impl_init`/`_deinit` exist, guarded by `ctx->event_subscribed`, called explicitly from `composition/main/application.c`). This phase brings `ota`, `messaging_callbacks`, and `presentation/mqtt/context` into the same shape, using `wifi_manager`'s implementation as the exact template. `settings` needs no change — audited and confirmed its `_new` already does only validate+alloc+memcpy with zero side effects, so it already matches the target shape as-is.

### Task 2: Split `ota`'s event-callback registration out of `_new` into `_init`/`_deinit`

**Files:**
- Modify: `main/include/application/internal/ota/impl.h`
- Modify: `main/include/application/internal/ota/impl_types.h`
- Modify: `main/src/application/internal/ota/impl.c`
- Modify: `main/src/composition/main/application.c`

**Interfaces:**
- Consumes: `dom_models_error_t ctx->cfg.system_update->add_event_callback(...)` / `remove_event_callback(...)` (unchanged, already used in current `_new`/`_delete`).
- Produces: `dom_models_error_t app_internal_ota_impl_init(dom_usecases_internal_ota_t* self)`, `void app_internal_ota_impl_deinit(dom_usecases_internal_ota_t* self)` — same shape as `app_internal_wifi_manager_impl_init`/`_deinit`.

- [ ] **Step 1: Add the `event_subscribed` flag to the ctx struct**

In `main/include/application/internal/ota/impl_types.h`, add `#include <stdbool.h>` near the top if not already present (it is not), and add a field to `app_internal_ota_impl_ctx_t`:
```c
typedef struct {
    app_internal_ota_impl_cfg_t cfg;
    bool                        updating;
    dom_models_error_t          last_result;
    int                         last_progress_percent;
    bool                        event_subscribed;
} app_internal_ota_impl_ctx_t;
```

- [ ] **Step 2: Declare `_init`/`_deinit` in the header**

In `main/include/application/internal/ota/impl.h`, add after `app_internal_ota_impl_delete`'s declaration:
```c
dom_models_error_t app_internal_ota_impl_init(dom_usecases_internal_ota_t* self);

void app_internal_ota_impl_deinit(dom_usecases_internal_ota_t* self);
```

- [ ] **Step 3: Move the registration out of `_new`, add `get_ctx` (ota doesn't have one yet — check first)**

`ota/impl.c` doesn't currently have a `get_ctx` helper (unlike `wifi_manager`/`settings`/`messaging_callbacks`) — it accesses `self->ctx` directly in each `*_impl` function. Add one for `_init`/`_deinit` to use, matching the exact shape used everywhere else in the codebase. Add this helper (with the other "Helper Function Prototypes" near the top, and its implementation in the "Helper Function Implementations" section — `ota/impl.c` doesn't have that section header yet either; add it at the end of the file):

Prototype (add near the existing `get_ctx`-less helper prototypes, i.e. right after the `on_update_event` prototype):
```c
static dom_models_error_t get_ctx(
    dom_usecases_internal_ota_t*  self,
    app_internal_ota_impl_ctx_t** out
);
```

Implementation (append at the end of the file, after `on_update_event`'s implementation):
```c
static dom_models_error_t get_ctx(
    dom_usecases_internal_ota_t*  self,
    app_internal_ota_impl_ctx_t** out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = self->ctx;

    return DOMAIN_MODELS_ERROR_OK;
}
```

Now replace `app_internal_ota_impl_new` (remove the registration block) — change:
```c
    self->update     = update_impl;
    self->validate   = validate_impl;
    self->rollback   = rollback_impl;
    self->get_status = get_status_impl;

    err = ctx->cfg.system_update->add_event_callback(ctx->cfg.system_update, ctx, on_update_event);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to register update event callback: %s (%d)", dom_models_error_str(err), (int)err);
        dom_usecases_internal_ota_delete(self);
        free(ctx);
        return NULL;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA created successfully");

    return self;
}
```
to:
```c
    self->update     = update_impl;
    self->validate   = validate_impl;
    self->rollback   = rollback_impl;
    self->get_status = get_status_impl;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA created successfully");

    return self;
}
```

Replace `app_internal_ota_impl_delete`'s teardown (remove the `remove_event_callback` call — it moves to `_deinit`) — change:
```c
    app_internal_ota_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        (void)ctx->cfg.system_update->remove_event_callback(ctx->cfg.system_update, on_update_event);
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA deleted successfully");
        free(ctx);
    }
```
to:
```c
    app_internal_ota_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA deleted successfully");
        free(ctx);
    }
```

Add `_init`/`_deinit` right after `app_internal_ota_impl_delete`'s closing brace, matching `wifi_manager`'s exact shape:
```c
dom_models_error_t app_internal_ota_impl_init(dom_usecases_internal_ota_t* self) {
    const char* tag = BASE_TAG "/init";

    app_internal_ota_impl_ctx_t* ctx = NULL;
    dom_models_error_t           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (ctx->event_subscribed) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    err = ctx->cfg.system_update->add_event_callback(ctx->cfg.system_update, ctx, on_update_event);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to register update event callback: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->event_subscribed = true;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void app_internal_ota_impl_deinit(dom_usecases_internal_ota_t* self) {
    const char* tag = BASE_TAG "/deinit";

    app_internal_ota_impl_ctx_t* ctx = NULL;
    if (get_ctx(self, &ctx) != DOMAIN_MODELS_ERROR_OK || !ctx->event_subscribed) {
        return;
    }

    (void)ctx->cfg.system_update->remove_event_callback(ctx->cfg.system_update, on_update_event);
    ctx->event_subscribed = false;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA deinitialized successfully");
}
```

- [ ] **Step 4: Wire `_init`/`_deinit` into composition**

In `main/src/composition/main/application.c`, add the `#include "application/internal/ota/impl.h"` is already present. Right after `launcher->application.ota = app_internal_ota_impl_new(&ota_cfg);` and its null-check, add:
```c
    dom_models_error_t err = app_internal_ota_impl_init(launcher->application.ota);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }
```
Note: the function already declares `dom_models_error_t err` later (from the wifi_manager init call) — since this new block is now the *first* use, move that declaration up: change the existing `dom_models_error_t err = app_internal_wifi_manager_impl_init(...)` a few lines down to `err = app_internal_wifi_manager_impl_init(...)` (drop the re-declaration, reuse `err`).

In `cmp_main_application_deinit`, right before the existing `if (launcher->application.ota) { app_internal_ota_impl_delete(...); }` block, add the deinit call:
```c
    if (launcher->application.ota) {
        app_internal_ota_impl_deinit(launcher->application.ota);
        app_internal_ota_impl_delete(launcher->application.ota);
        launcher->application.ota = NULL;
    }
```
(replacing the existing 3-line block that only called `_delete`).

- [ ] **Step 5: Build**

```bash
idf.py build
```
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add main/include/application/internal/ota/impl.h main/include/application/internal/ota/impl_types.h main/src/application/internal/ota/impl.c main/src/composition/main/application.c
git commit -m "refactor: split ota usecase's event-callback registration into init/deinit"
```

### Task 3: Split `messaging_callbacks`'s log-callback registration out of `_new` into `_init`/`_deinit`

**Files:**
- Modify: `main/include/application/internal/messaging_callbacks/impl.h`
- Modify: `main/include/application/internal/messaging_callbacks/impl_types.h`
- Modify: `main/src/application/internal/messaging_callbacks/impl.c`
- Modify: `main/src/composition/main/application.c`

**Interfaces:**
- Consumes: `dom_models_error_t ctx->cfg.logger->add_callback(...)` (now returning `dom_models_error_t`, per Task 1) / `void ctx->cfg.logger->remove_callback(...)`.
- Produces: `dom_models_error_t app_internal_messaging_callbacks_impl_init(dom_usecases_internal_messaging_callbacks_t* self)`, `void app_internal_messaging_callbacks_impl_deinit(dom_usecases_internal_messaging_callbacks_t* self)`.

Note: this task moves the registration from `logger->add_callback` directly to `_init`, but keeps calling `logger->add_callback` directly (not yet the `log_forwarding` usecase — that swap happens in Phase 3, Task 5, once this lifecycle split already exists for `_init`/`_deinit` to delegate from). This task is purely the mechanical lifecycle split, matching Task 2's ota pattern.

- [ ] **Step 1: Add a `log_cb_subscribed` flag to the ctx struct**

In `main/include/application/internal/messaging_callbacks/impl_types.h`, add `#include <stdbool.h>` and a field:
```c
typedef struct {
    app_internal_messaging_callbacks_impl_cfg_t cfg;
    char                                         device_id_str[37];
    bool                                          log_cb_subscribed;
} app_internal_messaging_callbacks_impl_ctx_t;
```

- [ ] **Step 2: Declare `_init`/`_deinit` in the header**

In `main/include/application/internal/messaging_callbacks/impl.h`, add:
```c
dom_models_error_t app_internal_messaging_callbacks_impl_init(dom_usecases_internal_messaging_callbacks_t* self);

void app_internal_messaging_callbacks_impl_deinit(dom_usecases_internal_messaging_callbacks_t* self);
```

- [ ] **Step 3: Move the registration out of `_new`/`_delete` into new `_init`/`_deinit`**

In `main/src/application/internal/messaging_callbacks/impl.c`, revert the Task-1 patch to `_new` (remove the now-relocated registration block) — change:
```c
    self->publish_registration  = publish_registration_impl;
    self->publish_online_status = publish_online_status_impl;
    self->subscribe_defaults    = subscribe_defaults_impl;
    self->restart                = restart_impl;
    self->publish_action_ack    = publish_action_ack_impl;

    err = ctx->cfg.logger->add_callback(ctx->cfg.logger, ctx, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to logger callbacks: %s (%d)", dom_models_error_str(err), (int)err);
        dom_usecases_internal_messaging_callbacks_delete(self);
        free(ctx);
        return NULL;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks created successfully");

    return self;
}
```
to:
```c
    self->publish_registration  = publish_registration_impl;
    self->publish_online_status = publish_online_status_impl;
    self->subscribe_defaults    = subscribe_defaults_impl;
    self->restart                = restart_impl;
    self->publish_action_ack    = publish_action_ack_impl;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks created successfully");

    return self;
}
```

Change `app_internal_messaging_callbacks_impl_delete` (remove the `remove_callback` call, it moves to `_deinit`) — change:
```c
    app_internal_messaging_callbacks_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->remove_callback(ctx->cfg.logger, on_log_message);
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks deleted successfully");
        free(ctx);
    }
```
to:
```c
    app_internal_messaging_callbacks_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks deleted successfully");
        free(ctx);
    }
```

Add `_init`/`_deinit` right after `app_internal_messaging_callbacks_impl_delete`'s closing brace:
```c
dom_models_error_t app_internal_messaging_callbacks_impl_init(dom_usecases_internal_messaging_callbacks_t* self) {
    const char* tag = BASE_TAG "/init";

    app_internal_messaging_callbacks_impl_ctx_t* ctx = NULL;
    dom_models_error_t                           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (ctx->log_cb_subscribed) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    err = ctx->cfg.logger->add_callback(ctx->cfg.logger, ctx, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to logger callbacks: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->log_cb_subscribed = true;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void app_internal_messaging_callbacks_impl_deinit(dom_usecases_internal_messaging_callbacks_t* self) {
    const char* tag = BASE_TAG "/deinit";

    app_internal_messaging_callbacks_impl_ctx_t* ctx = NULL;
    if (get_ctx(self, &ctx) != DOMAIN_MODELS_ERROR_OK || !ctx->log_cb_subscribed) {
        return;
    }

    ctx->cfg.logger->remove_callback(ctx->cfg.logger, on_log_message);
    ctx->log_cb_subscribed = false;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks deinitialized successfully");
}
```

- [ ] **Step 4: Wire `_init`/`_deinit` into composition**

In `main/src/composition/main/application.c`, right after `launcher->application.messaging_callbacks = app_internal_messaging_callbacks_impl_new(&messaging_callbacks_cfg);` and its null-check, add:
```c
    err = app_internal_messaging_callbacks_impl_init(launcher->application.messaging_callbacks);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }
```

In `cmp_main_application_deinit`, change:
```c
    if (launcher->application.messaging_callbacks) {
        app_internal_messaging_callbacks_impl_delete(launcher->application.messaging_callbacks);
        launcher->application.messaging_callbacks = NULL;
    }
```
to:
```c
    if (launcher->application.messaging_callbacks) {
        app_internal_messaging_callbacks_impl_deinit(launcher->application.messaging_callbacks);
        app_internal_messaging_callbacks_impl_delete(launcher->application.messaging_callbacks);
        launcher->application.messaging_callbacks = NULL;
    }
```

- [ ] **Step 5: Build**

```bash
idf.py build
```

- [ ] **Step 6: Commit**

```bash
git add main/include/application/internal/messaging_callbacks/impl.h main/include/application/internal/messaging_callbacks/impl_types.h main/src/application/internal/messaging_callbacks/impl.c main/src/composition/main/application.c
git commit -m "refactor: split messaging_callbacks usecase's log-callback registration into init/deinit"
```

### Task 4: Split `pres_mqtt_context`'s registration out of `_new`/`_delete` into `_init`/`_deinit`

**Files:**
- Modify: `main/include/presentation/mqtt/context.h`
- Modify: `main/src/presentation/mqtt/context.c`
- Modify: `main/src/composition/main/presentation.c`

**Interfaces:**
- Consumes: `esp_mqtt_client_register_event`/`esp_mqtt_client_unregister_event` (ESP-IDF's `mqtt_client.h`), `pres_mqtt_event_handler` (existing, from `presentation/mqtt/event/event_handler.h`).
- Produces: `dom_models_error_t pres_mqtt_context_init(pres_mqtt_context_t* self, esp_mqtt_client_handle_t mqtt_client)`, `void pres_mqtt_context_deinit(pres_mqtt_context_t* self, esp_mqtt_client_handle_t mqtt_client)`.

This is the biggest lifecycle-shape gap: `pres_mqtt_context_new` currently does `cfg`-field-by-field assignment (not even a single `memcpy`d cfg struct like every other module) plus a side effect (`get_device_id_str`), and the actual `esp_mqtt_client_register_event`/`unregister_event` pairing lives entirely outside `context.c`, in `composition/main/presentation.c`, with no `registered` guard. Bring it in line: `_new` becomes validate+alloc+assign only (the `get_device_id_str` call is arguably not a side effect in the "external registration" sense — it's a local, idempotent read with no observable effect outside this object, so it's fine to keep in `_new`, matching how `messaging_callbacks`'s own `_new` also reads `get_device_id_str` without controversy). What actually moves is the MQTT event registration.

- [ ] **Step 1: Add a `registered` flag and declare `_init`/`_deinit`**

In `main/include/presentation/mqtt/context.h`, add `#include <stdbool.h>` and a field, and the new declarations:
```c
#ifndef PRESENTATION_MQTT_CONTEXT_H
#define PRESENTATION_MQTT_CONTEXT_H

#include <stdbool.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/usecases/internal/messaging_callbacks.h"
#include "domain/usecases/internal/ota.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dom_contracts_logger_leveled_t*              logger;
    dom_contracts_repository_preloaded_t*        preloaded_repository;
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks;
    dom_usecases_internal_ota_t*                 ota;
    char                                         device_id_str[37];
    bool                                         registered;
} pres_mqtt_context_t;

pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_ota_t*                  ota
);

void pres_mqtt_context_delete(pres_mqtt_context_t* self);

dom_models_error_t pres_mqtt_context_init(pres_mqtt_context_t* self, esp_mqtt_client_handle_t mqtt_client);

void pres_mqtt_context_deinit(pres_mqtt_context_t* self, esp_mqtt_client_handle_t mqtt_client);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_CONTEXT_H */
```
(Note: `domain/models/error.h` for `dom_models_error_t` comes in transitively via `domain/usecases/internal/ota.h`, matching how the existing file already relies on that transitive include — no new include needed there.)

- [ ] **Step 2: Add `_init`/`_deinit` to `context.c`**

`main/src/presentation/mqtt/context.c` needs a `BASE_TAG` (it has none today — no per-file tag at all, since it only ever calls `free`/`calloc`, not the logger). Add one now since `_init`/`_deinit` will log, matching every other module's shape:

Replace the full file content:
```c
#include "presentation/mqtt/context.h"

#include <stdlib.h>

#include "domain/models/error.h"
#include "presentation/mqtt/event/event_handler.h"

#define BASE_TAG "pres_mqtt_context"

pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_ota_t*                 ota
) {
    if (!logger || !preloaded_repository || !messaging_callbacks || !ota) {
        return NULL;
    }

    pres_mqtt_context_t* self = (pres_mqtt_context_t*)calloc(1, sizeof(pres_mqtt_context_t));
    if (!self) {
        return NULL;
    }

    self->logger               = logger;
    self->preloaded_repository = preloaded_repository;
    self->messaging_callbacks  = messaging_callbacks;
    self->ota                  = ota;

    dom_models_error_t err = preloaded_repository->get_device_id_str(
        preloaded_repository,
        self->device_id_str,
        sizeof(self->device_id_str)
    );
    if (err != DOMAIN_MODELS_ERROR_OK) {
        free(self);
        return NULL;
    }

    logger->info(logger, BASE_TAG "/new", "MQTT context created successfully");

    return self;
}

void pres_mqtt_context_delete(pres_mqtt_context_t* self) {
    if (!self) {
        return;
    }

    free(self);
}

dom_models_error_t pres_mqtt_context_init(pres_mqtt_context_t* self, esp_mqtt_client_handle_t mqtt_client) {
    const char* tag = BASE_TAG "/init";

    if (!self || !mqtt_client) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    if (self->registered) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    esp_err_t esp_err = esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        pres_mqtt_event_handler,
        self
    );
    if (esp_err != ESP_OK) {
        self->logger->error(self->logger, tag, "Failed to register MQTT event handler: %d", (int)esp_err);
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    self->registered = true;

    self->logger->info(self->logger, tag, "MQTT context initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_mqtt_context_deinit(pres_mqtt_context_t* self, esp_mqtt_client_handle_t mqtt_client) {
    if (!self || !mqtt_client || !self->registered) {
        return;
    }

    esp_mqtt_client_unregister_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        pres_mqtt_event_handler
    );

    self->registered = false;
}
```
(`ESP_EVENT_ANY_ID` and `esp_err_t`/`ESP_OK` come from `mqtt_client.h`/ESP-IDF's event system, already transitively available the same way `composition/main/presentation.c` currently gets them.)

- [ ] **Step 3: Update composition wiring**

In `main/src/composition/main/presentation.c`, replace:
```c
    launcher->presentation.mqtt_context = pres_mqtt_context_new(
        launcher->infrastructure.logger,
        launcher->infrastructure.preloaded_repository,
        launcher->application.messaging_callbacks,
        launcher->application.ota
    );
    if (!launcher->presentation.mqtt_context) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    esp_err_t esp_err = esp_mqtt_client_register_event(
        launcher->driver.mqtt_client,
        ESP_EVENT_ANY_ID,
        pres_mqtt_event_handler,
        launcher->presentation.mqtt_context
    );
    if (esp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }
```
with:
```c
    launcher->presentation.mqtt_context = pres_mqtt_context_new(
        launcher->infrastructure.logger,
        launcher->infrastructure.preloaded_repository,
        launcher->application.messaging_callbacks,
        launcher->application.ota
    );
    if (!launcher->presentation.mqtt_context) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    dom_models_error_t err = pres_mqtt_context_init(launcher->presentation.mqtt_context, launcher->driver.mqtt_client);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }
```
Note: `cmp_main_presentation_init` already declares a later `dom_models_error_t err = pres_task_wifi_sta_reconnect_start(...)` — since this new block is now the first use, change that later line from `dom_models_error_t err = pres_task_wifi_sta_reconnect_start(...)` to `err = pres_task_wifi_sta_reconnect_start(...)` (drop the re-declaration).

In `cmp_main_presentation_deinit`, replace:
```c
    if (launcher->driver.mqtt_client) {
        esp_mqtt_client_unregister_event(
            launcher->driver.mqtt_client,
            ESP_EVENT_ANY_ID,
            pres_mqtt_event_handler
        );
    }

    if (launcher->presentation.mqtt_context) {
        pres_mqtt_context_delete(launcher->presentation.mqtt_context);
        launcher->presentation.mqtt_context = NULL;
    }
```
with:
```c
    if (launcher->presentation.mqtt_context) {
        pres_mqtt_context_deinit(launcher->presentation.mqtt_context, launcher->driver.mqtt_client);
        pres_mqtt_context_delete(launcher->presentation.mqtt_context);
        launcher->presentation.mqtt_context = NULL;
    }
```
(The `mqtt_client.h` include in `presentation.c` stays — `esp_mqtt_client_register_event`/`unregister_event` calls are gone from this file, but `launcher->driver.mqtt_client`'s type `esp_mqtt_client_handle_t` is still referenced.)

- [ ] **Step 4: Build**

```bash
idf.py build
```

- [ ] **Step 5: Commit**

```bash
git add main/include/presentation/mqtt/context.h main/src/presentation/mqtt/context.c main/src/composition/main/presentation.c
git commit -m "refactor: split mqtt context's event registration into init/deinit, matching the rest of the codebase's module lifecycle shape"
```

### Phase 2 checkpoint

- [ ] Full `idf.py build` clean.
- [ ] Flash to the real device (`idf.py -p /dev/ttyACM0 flash`) and capture ~10s of boot log (same technique as `docs/agent_test/v1.0.0-dev.1/scenario/00-setup.md`'s "Monitoring the device" section) — confirm the exact same sequence of `"... created successfully"` / `"... initialized successfully"` log lines still appears in the same order as before this phase (OTA, Settings, WiFi manager created+initialized, Messaging callbacks created+initialized, WiFi started, BLE handlers initialized, BLE advertising started), and that MQTT still connects and registers (`[pres_mqtt_on_connect] Connected to MQTT broker`, `Registration published successfully`). This is a pure lifecycle-shape refactor — any behavior difference here is a bug.

---

## Phase 3 — Extract shared `log_forwarding` usecase

### Task 5: Create `domain/usecases/internal/log_forwarding` + `application/internal/log_forwarding` (impl)

**Files:**
- Create: `main/include/domain/usecases/internal/log_forwarding.h`
- Create: `main/include/application/internal/log_forwarding/impl_types.h`
- Create: `main/include/application/internal/log_forwarding/impl_utils.h`
- Create: `main/include/application/internal/log_forwarding/impl.h`
- Create: `main/src/application/internal/log_forwarding/impl_utils.c`
- Create: `main/src/application/internal/log_forwarding/impl.c`

**Interfaces:**
- Produces (domain contract, consumed by Task 6):
  - `dom_usecases_internal_log_forwarding_t` with:
    - `dom_models_error_t (*add_sink)(dom_usecases_internal_log_forwarding_t* self, void* cb_ctx, dom_contracts_logger_leveled_cb cb_func)`
    - `dom_models_error_t (*remove_sink)(dom_usecases_internal_log_forwarding_t* self, dom_contracts_logger_leveled_cb cb_func)`
    (reuses the existing `dom_contracts_logger_leveled_cb` signature — `void (*)(void* cb_ctx, const char* msg, size_t msg_len)` — rather than defining a new duplicate typedef, since it's the exact same shape.)
  - `app_internal_log_forwarding_impl_new(cfg)` / `_delete(self)` / `_init(self)` / `_deinit(self)` — same lifecycle shape as Phase 2.

- [ ] **Step 1: Domain usecase interface**

Create `main/include/domain/usecases/internal/log_forwarding.h`:
```c
#ifndef DOMAIN_USECASES_INTERNAL_LOG_FORWARDING_H
#define DOMAIN_USECASES_INTERNAL_LOG_FORWARDING_H

#include <stdlib.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_usecases_internal_log_forwarding_t dom_usecases_internal_log_forwarding_t;

/* Fans out every logger line to whichever transports (MQTT, BLE, ...) have
   registered a sink - the single point through which any transport gets
   log lines, so the layering rule is the same for every transport instead
   of each one deciding independently whether to subscribe to
   dom_contracts_logger_leveled_t directly. */
struct dom_usecases_internal_log_forwarding_t {
    void* ctx;
    dom_models_error_t (*add_sink)(
        dom_usecases_internal_log_forwarding_t* self,
        void*                                   cb_ctx,
        dom_contracts_logger_leveled_cb         cb_func
    );
    dom_models_error_t (*remove_sink)(
        dom_usecases_internal_log_forwarding_t* self,
        dom_contracts_logger_leveled_cb         cb_func
    );
};

static inline dom_usecases_internal_log_forwarding_t* dom_usecases_internal_log_forwarding_new(void* ctx) {
    dom_usecases_internal_log_forwarding_t* self = (dom_usecases_internal_log_forwarding_t*)calloc(1, sizeof(dom_usecases_internal_log_forwarding_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_log_forwarding_delete(dom_usecases_internal_log_forwarding_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_LOG_FORWARDING_H */
```

- [ ] **Step 2: `impl_types.h`**

Create `main/include/application/internal/log_forwarding/impl_types.h`:
```c
#ifndef APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_TYPES_H
#define APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_TYPES_H

#include <stdbool.h>

#include "domain/contracts/logger/leveled.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Two known consumers today: BLE's log handler and MQTT's messaging_callbacks. */
#define APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_SINK_CB_MAX_CNT 2

typedef struct {
    dom_contracts_logger_leveled_t* logger;
} app_internal_log_forwarding_impl_cfg_t;

typedef struct {
    app_internal_log_forwarding_impl_cfg_t cfg;
    bool                                   subscribed;
    dom_contracts_logger_leveled_cb        sink_cb_funcs[APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_SINK_CB_MAX_CNT];
    void*                                  sink_cb_ctxs[APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_SINK_CB_MAX_CNT];
    unsigned int                           sink_cb_idx;
} app_internal_log_forwarding_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_TYPES_H */
```

- [ ] **Step 3: `impl_utils.h`/`.c` (`validate_cfg`)**

Create `main/include/application/internal/log_forwarding/impl_utils.h`:
```c
#ifndef APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_UTILS_H
#define APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_UTILS_H

#include "application/internal/log_forwarding/impl_types.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_log_forwarding_impl_validate_cfg(const app_internal_log_forwarding_impl_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_UTILS_H */
```

Create `main/src/application/internal/log_forwarding/impl_utils.c`:
```c
#include "application/internal/log_forwarding/impl_utils.h"

dom_models_error_t app_internal_log_forwarding_impl_validate_cfg(const app_internal_log_forwarding_impl_cfg_t* cfg) {
    if (!cfg ||
        !cfg->logger ||
        !cfg->logger->error ||
        !cfg->logger->info ||
        !cfg->logger->add_callback ||
        !cfg->logger->remove_callback) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 4: `impl.h`**

Create `main/include/application/internal/log_forwarding/impl.h`:
```c
#ifndef APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_H
#define APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_H

#include "application/internal/log_forwarding/impl_types.h"
#include "domain/usecases/internal/log_forwarding.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_log_forwarding_t* app_internal_log_forwarding_impl_new(const app_internal_log_forwarding_impl_cfg_t* cfg);

void app_internal_log_forwarding_impl_delete(dom_usecases_internal_log_forwarding_t* self);

dom_models_error_t app_internal_log_forwarding_impl_init(dom_usecases_internal_log_forwarding_t* self);

void app_internal_log_forwarding_impl_deinit(dom_usecases_internal_log_forwarding_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_H */
```

- [ ] **Step 5: `impl.c`**

Create `main/src/application/internal/log_forwarding/impl.c`:
```c
#include "application/internal/log_forwarding/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/log_forwarding/impl_types.h"
#include "application/internal/log_forwarding/impl_utils.h"
#include "domain/models/error.h"
#include "domain/usecases/internal/log_forwarding.h"

#define BASE_TAG "internal_log_forwarding"

/* Helper Function Prototypes */

static dom_models_error_t get_ctx(
    dom_usecases_internal_log_forwarding_t*  self,
    app_internal_log_forwarding_impl_ctx_t** out
);

static void on_log_message(void* cb_ctx, const char* msg, size_t msg_len);

/* Contract Function Prototypes */

static dom_models_error_t add_sink_impl(
    dom_usecases_internal_log_forwarding_t* self,
    void*                                   cb_ctx,
    dom_contracts_logger_leveled_cb         cb_func
);
static dom_models_error_t remove_sink_impl(
    dom_usecases_internal_log_forwarding_t* self,
    dom_contracts_logger_leveled_cb         cb_func
);

/* Constructor and Destructor */

dom_usecases_internal_log_forwarding_t* app_internal_log_forwarding_impl_new(const app_internal_log_forwarding_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    dom_models_error_t err = app_internal_log_forwarding_impl_validate_cfg(cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_log_forwarding_impl_ctx_t* ctx = (app_internal_log_forwarding_impl_ctx_t*)calloc(1, sizeof(app_internal_log_forwarding_impl_ctx_t));
    if (!ctx) {
        cfg->logger->error(cfg->logger, tag, "Failed to allocate log forwarding context: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_log_forwarding_impl_cfg_t));

    dom_usecases_internal_log_forwarding_t* self = dom_usecases_internal_log_forwarding_new(ctx);
    if (!self) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to allocate log forwarding usecase: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        free(ctx);
        return NULL;
    }

    self->add_sink    = add_sink_impl;
    self->remove_sink = remove_sink_impl;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Log forwarding created successfully");

    return self;
}

void app_internal_log_forwarding_impl_delete(dom_usecases_internal_log_forwarding_t* self) {
    const char* tag = BASE_TAG "/delete";

    if (!self) {
        return;
    }

    app_internal_log_forwarding_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Log forwarding deleted successfully");
        free(ctx);
    }

    dom_usecases_internal_log_forwarding_delete(self);
}

dom_models_error_t app_internal_log_forwarding_impl_init(dom_usecases_internal_log_forwarding_t* self) {
    const char* tag = BASE_TAG "/init";

    app_internal_log_forwarding_impl_ctx_t* ctx = NULL;
    dom_models_error_t                      err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (ctx->subscribed) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    err = ctx->cfg.logger->add_callback(ctx->cfg.logger, ctx, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to logger callbacks: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->subscribed = true;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Log forwarding initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void app_internal_log_forwarding_impl_deinit(dom_usecases_internal_log_forwarding_t* self) {
    const char* tag = BASE_TAG "/deinit";

    app_internal_log_forwarding_impl_ctx_t* ctx = NULL;
    if (get_ctx(self, &ctx) != DOMAIN_MODELS_ERROR_OK || !ctx->subscribed) {
        return;
    }

    ctx->cfg.logger->remove_callback(ctx->cfg.logger, on_log_message);
    ctx->subscribed = false;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Log forwarding deinitialized successfully");
}

/* Contract Function Implementations */

static dom_models_error_t add_sink_impl(
    dom_usecases_internal_log_forwarding_t* self,
    void*                                   cb_ctx,
    dom_contracts_logger_leveled_cb         cb_func
) {
    const char* tag = BASE_TAG "/add_sink";

    app_internal_log_forwarding_impl_ctx_t* ctx = NULL;
    dom_models_error_t                      err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!cb_func) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing sink callback function: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    if (ctx->sink_cb_idx >= APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_SINK_CB_MAX_CNT) {
        err = DOMAIN_MODELS_ERROR_BAD_STATE;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "No room left for another log forwarding sink: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->sink_cb_funcs[ctx->sink_cb_idx] = cb_func;
    ctx->sink_cb_ctxs[ctx->sink_cb_idx]  = cb_ctx;
    ctx->sink_cb_idx += 1;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t remove_sink_impl(
    dom_usecases_internal_log_forwarding_t* self,
    dom_contracts_logger_leveled_cb         cb_func
) {
    app_internal_log_forwarding_impl_ctx_t* ctx = NULL;
    dom_models_error_t                      err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!cb_func || ctx->sink_cb_idx == 0) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    for (unsigned int i = 0; i < ctx->sink_cb_idx; i++) {
        if (ctx->sink_cb_funcs[i] != cb_func) {
            continue;
        }

        unsigned int last_idx = ctx->sink_cb_idx - 1;

        ctx->sink_cb_funcs[i] = NULL;
        ctx->sink_cb_ctxs[i]  = NULL;

        if (i != last_idx) {
            ctx->sink_cb_funcs[i] = ctx->sink_cb_funcs[last_idx];
            ctx->sink_cb_ctxs[i]  = ctx->sink_cb_ctxs[last_idx];

            ctx->sink_cb_funcs[last_idx] = NULL;
            ctx->sink_cb_ctxs[last_idx]  = NULL;
        }

        ctx->sink_cb_idx -= 1;
        return DOMAIN_MODELS_ERROR_OK;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static dom_models_error_t get_ctx(
    dom_usecases_internal_log_forwarding_t*  self,
    app_internal_log_forwarding_impl_ctx_t** out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = self->ctx;

    return DOMAIN_MODELS_ERROR_OK;
}

/* Runs synchronously on whichever task called logger->error/warn/info/
   debug(...) - never call back into ctx->cfg.logger here, since this
   callback is registered on that same logger instance and logging from
   within it would re-enter print_log() -> run_callbacks() -> this
   function, recursing without end (same rule messaging_callbacks'
   on_log_message documented before this usecase existed). */
static void on_log_message(void* cb_ctx, const char* msg, size_t msg_len) {
    app_internal_log_forwarding_impl_ctx_t* ctx = cb_ctx;
    if (!ctx || !msg || msg_len == 0) {
        return;
    }

    for (unsigned int i = 0; i < ctx->sink_cb_idx; i++) {
        if (ctx->sink_cb_funcs[i]) {
            ctx->sink_cb_funcs[i](ctx->sink_cb_ctxs[i], msg, msg_len);
        }
    }
}
```

- [ ] **Step 6: Build**

```bash
idf.py build
```
Expected: clean build. This task only adds new files — nothing references them yet, so nothing else should change behavior.

- [ ] **Step 7: Commit**

```bash
git add main/include/domain/usecases/internal/log_forwarding.h main/include/application/internal/log_forwarding/ main/src/application/internal/log_forwarding/
git commit -m "feat: add log_forwarding usecase as the single place transports subscribe to logger callbacks"
```

### Task 6: Rewire `messaging_callbacks` and BLE's `log` handler to consume `log_forwarding` instead of the logger directly

**Files:**
- Modify: `main/include/application/internal/messaging_callbacks/impl_types.h`
- Modify: `main/include/application/internal/messaging_callbacks/impl_utils.h`
- Modify: `main/src/application/internal/messaging_callbacks/impl.c`
- Modify: `main/src/application/internal/messaging_callbacks/impl_utils.c`
- Modify: `main/include/presentation/ble/handler/log/types.h`
- Modify: `main/src/presentation/ble/handler/log/handler.c`
- Modify: `main/include/composition/main/types.h`
- Modify: `main/src/composition/main/application.c`
- Modify: `main/src/composition/main/presentation.c`

**Interfaces:**
- Consumes: `dom_usecases_internal_log_forwarding_t.add_sink`/`.remove_sink` from Task 5.

- [ ] **Step 1: `messaging_callbacks` gains a `log_forwarding` cfg dependency**

In `main/include/application/internal/messaging_callbacks/impl_types.h`, add the include and cfg field:
```c
#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/messaging/def_pub.h"
#include "domain/contracts/messaging/def_sub.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/system/info.h"
#include "domain/contracts/system/restart.h"
#include "domain/usecases/internal/log_forwarding.h"
```
and in the cfg struct:
```c
typedef struct {
    dom_contracts_logger_leveled_t*         logger;
    dom_contracts_messaging_def_pub_t*      def_pub;
    dom_contracts_messaging_def_sub_t*      def_sub;
    dom_contracts_system_restart_t*         system_restart;
    dom_contracts_repository_preloaded_t*   preloaded_repository;
    dom_contracts_system_info_t*            system_info;
    dom_usecases_internal_log_forwarding_t* log_forwarding;
} app_internal_messaging_callbacks_impl_cfg_t;
```

- [ ] **Step 2: `validate_cfg` checks the new dependency**

In `main/src/application/internal/messaging_callbacks/impl_utils.c`, add to the `if` condition in `app_internal_messaging_callbacks_impl_validate_cfg`:
```c
    if (!cfg ||
        !cfg->logger ||
        !cfg->logger->error ||
        !cfg->logger->info ||
        !has_def_pub_functions(cfg->def_pub) ||
        !has_def_sub_functions(cfg->def_sub) ||
        !has_system_restart_functions(cfg->system_restart) ||
        !has_preloaded_repository_functions(cfg->preloaded_repository) ||
        !has_system_info_functions(cfg->system_info) ||
        !cfg->log_forwarding ||
        !cfg->log_forwarding->add_sink ||
        !cfg->log_forwarding->remove_sink) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
```
(Note: `cfg->logger->add_callback`/`remove_callback` are dropped from this check — `messaging_callbacks` no longer calls those directly.)

- [ ] **Step 3: Swap the registration call in `impl.c`**

In `main/src/application/internal/messaging_callbacks/impl.c`, in `app_internal_messaging_callbacks_impl_init` (added in Task 3), change:
```c
    err = ctx->cfg.logger->add_callback(ctx->cfg.logger, ctx, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to logger callbacks: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }
```
to:
```c
    err = ctx->cfg.log_forwarding->add_sink(ctx->cfg.log_forwarding, ctx, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to log forwarding: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }
```
In `app_internal_messaging_callbacks_impl_deinit`, change:
```c
    ctx->cfg.logger->remove_callback(ctx->cfg.logger, on_log_message);
```
to:
```c
    ctx->cfg.log_forwarding->remove_sink(ctx->cfg.log_forwarding, on_log_message);
```
`on_log_message`'s own body and its recursion-avoidance comment stay exactly as-is — the fan-out source changed, the sink function did not.

- [ ] **Step 4: BLE log handler gains a `log_forwarding` cfg dependency**

In `main/include/presentation/ble/handler/log/types.h`, add the include and cfg field — change:
```c
#include "domain/contracts/logger/leveled.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"
#include "presentation/ble/gatt/registry.h"
#include "presentation/ble/host.h"
```
to:
```c
#include "domain/contracts/logger/leveled.h"
#include "domain/usecases/internal/log_forwarding.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"
#include "presentation/ble/gatt/registry.h"
#include "presentation/ble/host.h"
```
and:
```c
typedef struct {
    dom_contracts_logger_leveled_t*         logger;
    dom_usecases_internal_log_forwarding_t* log_forwarding;
    pres_ble_gatt_registry_t*               gatt_registry;
    pres_ble_host_t*                        host;
} pres_ble_handler_log_cfg_t;
```
(`logger` stays — the handler's own `_init`/`_deinit`/access-callback functions still call `self->cfg.logger->error/info` for their own logging; only the *subscription* moves to `log_forwarding`.)

- [ ] **Step 5: Swap the registration call in `handler.c`**

In `main/src/presentation/ble/handler/log/handler.c`, `pres_ble_handler_log_init` currently (after Task 1's temporary patch) has:
```c
    err = self->cfg.logger->add_callback(self->cfg.logger, self, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to subscribe to logger callbacks: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }
    self->logger_cb_subscribed = true;
```
Change to:
```c
    err = self->cfg.log_forwarding->add_sink(self->cfg.log_forwarding, self, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to subscribe to log forwarding: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }
    self->logger_cb_subscribed = true;
```
In `pres_ble_handler_log_deinit`, change:
```c
    if (self->logger_cb_subscribed) {
        self->cfg.logger->remove_callback(self->cfg.logger, on_log_message);
        self->logger_cb_subscribed = false;
    }
```
to:
```c
    if (self->logger_cb_subscribed) {
        self->cfg.log_forwarding->remove_sink(self->cfg.log_forwarding, on_log_message);
        self->logger_cb_subscribed = false;
    }
```
`on_log_message`'s body (the queue-send into the dedicated `ble_log` task) is unchanged.

Also update `pres_ble_handler_log_new`'s validation — it currently checks `!cfg->logger`; add the new dependency:
```c
pres_ble_handler_log_t* pres_ble_handler_log_new(const pres_ble_handler_log_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->log_forwarding || !cfg->gatt_registry || !cfg->host) {
        return NULL;
    }
```

- [ ] **Step 6: Composition — construct `log_forwarding`, init it, wire it into both consumers**

In `main/include/composition/main/types.h`, add the include and application-struct field:
```c
#include "domain/usecases/internal/log_forwarding.h"
```
(alphabetically among the existing `domain/usecases/internal/*` includes, i.e. right before `#include "domain/usecases/internal/messaging_callbacks.h"`), and:
```c
typedef struct {
    dom_usecases_internal_ota_t*                 ota;
    dom_usecases_internal_settings_t*            settings;
    dom_usecases_internal_wifi_manager_t*        wifi_manager;
    dom_usecases_internal_log_forwarding_t*      log_forwarding;
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks;
} cmp_main_launcher_application_t;
```

In `main/src/composition/main/application.c`, add `#include "application/internal/log_forwarding/impl.h"` to the includes, and construct+init `log_forwarding` **before** `messaging_callbacks` (since `messaging_callbacks_cfg` will reference it) — insert right before the existing `app_internal_messaging_callbacks_impl_cfg_t messaging_callbacks_cfg = {` block:
```c
    app_internal_log_forwarding_impl_cfg_t log_forwarding_cfg = {
        .logger = launcher->infrastructure.logger,
    };
    launcher->application.log_forwarding = app_internal_log_forwarding_impl_new(&log_forwarding_cfg);
    if (!launcher->application.log_forwarding) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    err = app_internal_log_forwarding_impl_init(launcher->application.log_forwarding);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

```
Then add `.log_forwarding = launcher->application.log_forwarding,` to the existing `messaging_callbacks_cfg` initializer.

In `cmp_main_application_deinit`, add teardown — insert right after the existing `messaging_callbacks` teardown block (log_forwarding must outlive messaging_callbacks and BLE's log handler, since both hold a pointer to it and call `remove_sink` during their own deinit — so it must be deinit/deleted *last*, after both consumers):
```c
    if (launcher->application.log_forwarding) {
        app_internal_log_forwarding_impl_deinit(launcher->application.log_forwarding);
        app_internal_log_forwarding_impl_delete(launcher->application.log_forwarding);
        launcher->application.log_forwarding = NULL;
    }
```
(This relies on presentation being deinitialized before application in `cmp_main_launcher`'s existing `fail:` unwind order, which is already the case — `cmp_main_presentation_deinit` runs before `cmp_main_application_deinit`.)

In `main/src/composition/main/presentation.c`, add `.log_forwarding = launcher->application.log_forwarding,` to the existing `ble_log_cfg` initializer:
```c
    pres_ble_handler_log_cfg_t ble_log_cfg = {
        .logger         = launcher->infrastructure.logger,
        .log_forwarding = launcher->application.log_forwarding,
        .gatt_registry  = launcher->presentation.ble_gatt_registry,
        .host           = launcher->presentation.ble_host,
    };
```

- [ ] **Step 7: Build**

```bash
idf.py build
```
Expected: clean build.

- [ ] **Step 8: Commit**

```bash
git add main/include/application/internal/messaging_callbacks/impl_types.h main/include/application/internal/messaging_callbacks/impl_utils.h main/src/application/internal/messaging_callbacks/impl.c main/src/application/internal/messaging_callbacks/impl_utils.c main/include/presentation/ble/handler/log/types.h main/src/presentation/ble/handler/log/handler.c main/include/composition/main/types.h main/src/composition/main/application.c main/src/composition/main/presentation.c
git commit -m "refactor: route MQTT and BLE log forwarding through the shared log_forwarding usecase instead of subscribing to the logger directly"
```

### Phase 3 checkpoint (hardware-in-loop)

- [ ] Flash to the real device. Re-run the MQTT log-over-broker check from `docs/agent_test/v1.0.0-dev.1/scenario/04-logging.md` (`LOG-01`/`LOG-02`) — confirm device log lines still arrive on `/pub/<device_id>/log` via `mosquitto_sub`.
- [ ] Re-run the BLE log-notification path from `docs/agent_test/v1.0.0-dev.1/scenario/10-ble-gatt-services.md` (enable the log service's `enabled` characteristic, subscribe to `message` notifications, trigger some log activity) — confirm log lines still arrive over BLE. This exercises both consumers of the new shared usecase in the same pass.

---

## Phase 4 — MQTT DTO extraction

Restructures `presentation/mqtt/handler/{action,ota}.c` (flat files) into `handler/{action,ota}/{handler,dto}.{c,h}` subdirectories, matching BLE's `handler/<name>/{handler,dto,types}.{c,h}` layout. `registration_ack` stays flat — it does no payload parsing, so it needs no `dto.c` and there's no reason to introduce a subdirectory with only a `handler.c` in it.

### Task 7: Extract `action`'s DTO into its own subdirectory

**Files:**
- Delete: `main/src/presentation/mqtt/handler/action.c`
- Delete: `main/include/presentation/mqtt/handler/action.h`
- Create: `main/src/presentation/mqtt/handler/action/handler.c`
- Create: `main/src/presentation/mqtt/handler/action/dto.c`
- Create: `main/include/presentation/mqtt/handler/action/handler.h`
- Create: `main/include/presentation/mqtt/handler/action/dto.h`
- Modify: `main/src/presentation/mqtt/event/on_message.c` (include path + call site — call site itself unchanged, `pres_mqtt_handler_action(...)` keeps its name/signature)

**Interfaces:**
- Produces: `dom_models_error_t pres_mqtt_handler_action_dto_decode(const char* data, int data_len, char* execution_id_out, size_t execution_id_out_size, char* action_out, size_t action_out_size, uint32_t* delay_ms_out, bool* delay_ms_set_out)` — decodes the incoming JSON into fixed caller-owned buffers (matching BLE's dto.c convention of caller-owned output, no heap allocation returned to the caller).
- Consumes (unchanged): `dom_usecases_internal_messaging_callbacks_t.publish_action_ack`/`.restart`.

- [ ] **Step 1: Write the DTO header**

Create `main/include/presentation/mqtt/handler/action/dto.h`:
```c
#ifndef PRESENTATION_MQTT_HANDLER_ACTION_DTO_H
#define PRESENTATION_MQTT_HANDLER_ACTION_DTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_MQTT_HANDLER_ACTION_DTO_EXECUTION_ID_MAX_LEN 64
#define PRES_MQTT_HANDLER_ACTION_DTO_ACTION_MAX_LEN        32

typedef struct {
    char     execution_id[PRES_MQTT_HANDLER_ACTION_DTO_EXECUTION_ID_MAX_LEN];
    bool     execution_id_set;
    char     action[PRES_MQTT_HANDLER_ACTION_DTO_ACTION_MAX_LEN];
    bool     action_set;
    uint32_t delay_ms;
} pres_mqtt_handler_action_dto_request_t;

/* Parses the /sub/<device_id>/action payload. Returns
   DOMAIN_MODELS_ERROR_BAD_ARGUMENT if data is empty/not valid JSON -
   individual missing-field cases are reported via the _set flags instead
   of a decode error, matching the existing negative-case behavior
   (ACT-05/ACT-06 in docs/agent_test/v1.0.0-dev.1/scenario/05-action-dispatch.md)
   where a missing execution_id is silently dropped by the caller and a
   missing action field gets an explicit FAILED ack. */
dom_models_error_t pres_mqtt_handler_action_dto_decode(
    const char*                             data,
    int                                     data_len,
    pres_mqtt_handler_action_dto_request_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_ACTION_DTO_H */
```

- [ ] **Step 2: Write the DTO implementation**

Create `main/src/presentation/mqtt/handler/action/dto.c`:
```c
#include "presentation/mqtt/handler/action/dto.h"

#include <string.h>

#include "cJSON.h"

dom_models_error_t pres_mqtt_handler_action_dto_decode(
    const char*                             data,
    int                                     data_len,
    pres_mqtt_handler_action_dto_request_t* out
) {
    if (!data || data_len <= 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));

    cJSON* json = cJSON_ParseWithLength(data, (size_t)data_len);
    if (!json) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* execution_id_item = cJSON_GetObjectItemCaseSensitive(json, "execution_id");
    cJSON* action_item       = cJSON_GetObjectItemCaseSensitive(json, "action");
    cJSON* payload_item      = cJSON_GetObjectItemCaseSensitive(json, "payload");

    if (cJSON_IsString(execution_id_item) && execution_id_item->valuestring) {
        strncpy(out->execution_id, execution_id_item->valuestring, sizeof(out->execution_id) - 1);
        out->execution_id_set = true;
    }

    if (cJSON_IsString(action_item) && action_item->valuestring) {
        strncpy(out->action, action_item->valuestring, sizeof(out->action) - 1);
        out->action_set = true;
    }

    cJSON* delay_ms_item = cJSON_IsObject(payload_item) ? cJSON_GetObjectItemCaseSensitive(payload_item, "delay_ms") : NULL;
    out->delay_ms        = cJSON_IsNumber(delay_ms_item) ? (uint32_t)delay_ms_item->valuedouble : 0;

    cJSON_Delete(json);

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 3: Write the new handler header (same public signature as before)**

Create `main/include/presentation/mqtt/handler/action/handler.h`:
```c
#ifndef PRESENTATION_MQTT_HANDLER_ACTION_HANDLER_H
#define PRESENTATION_MQTT_HANDLER_ACTION_HANDLER_H

#include "presentation/mqtt/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void pres_mqtt_handler_action(pres_mqtt_context_t* ctx, const char* data, int data_len);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_ACTION_HANDLER_H */
```

- [ ] **Step 4: Write the new handler implementation (delegates decoding to dto.c)**

Create `main/src/presentation/mqtt/handler/action/handler.c`:
```c
#include "presentation/mqtt/handler/action/handler.h"

#include <string.h>

#include "domain/models/error.h"
#include "presentation/mqtt/handler/action/dto.h"

#define BASE_TAG "pres_mqtt_action"

#define ACTION_RESTART "restart"

#define ACTION_ACK_STATUS_SUCCESS "SUCCESS"
#define ACTION_ACK_STATUS_FAILED  "FAILED"

void pres_mqtt_handler_action(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->info(ctx->logger, tag, "Received action request via MQTT");

    pres_mqtt_handler_action_dto_request_t request;
    dom_models_error_t                     err = pres_mqtt_handler_action_dto_decode(data, data_len, &request);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, tag, "Failed to parse action payload: %s (%d)", dom_models_error_str(err), (int)err);
        return;
    }

    if (!request.execution_id_set) {
        ctx->logger->error(ctx->logger, tag, "Invalid action payload: missing execution_id field");
        return;
    }

    if (!request.action_set) {
        ctx->logger->error(ctx->logger, tag, "Invalid action payload: missing action field");
        (void)ctx->messaging_callbacks->publish_action_ack(ctx->messaging_callbacks, request.execution_id, ACTION_ACK_STATUS_FAILED, "missing action field");
        return;
    }

    if (strcmp(request.action, ACTION_RESTART) == 0) {
        (void)ctx->messaging_callbacks->publish_action_ack(ctx->messaging_callbacks, request.execution_id, ACTION_ACK_STATUS_SUCCESS, NULL);

        err = ctx->messaging_callbacks->restart(ctx->messaging_callbacks, request.delay_ms);
        if (err == DOMAIN_MODELS_ERROR_OK) {
            ctx->logger->info(ctx->logger, tag, "Restart action requested successfully");
        }
        /* No error log on failure here - the usecase (messaging_callbacks'
           restart_impl) already logs the failure; re-logging the same
           condition here would double-log it (see Phase 6). */
    } else {
        ctx->logger->warn(ctx->logger, tag, "Unknown action: %s", request.action);
        (void)ctx->messaging_callbacks->publish_action_ack(ctx->messaging_callbacks, request.execution_id, ACTION_ACK_STATUS_FAILED, "unknown action");
    }
}
```
(This step already strips the redundant error log that Phase 6 would otherwise remove separately — since this file is being rewritten wholesale here anyway, doing it now avoids a needless extra edit pass later. Phase 6's task for `action.c` becomes a no-op confirmation step, noted there.)

- [ ] **Step 5: Delete the old flat files**

```bash
git rm main/src/presentation/mqtt/handler/action.c main/include/presentation/mqtt/handler/action.h
```

- [ ] **Step 6: Update the include path in `on_message.c`**

In `main/src/presentation/mqtt/event/on_message.c`, change:
```c
#include "presentation/mqtt/handler/action.h"
```
to:
```c
#include "presentation/mqtt/handler/action/handler.h"
```
(The call site `pres_mqtt_handler_action(ctx, event->data, event->data_len);` is unchanged — same function name/signature.)

- [ ] **Step 7: Build**

```bash
idf.py build
```
Expected: clean build.

- [ ] **Step 8: Commit**

```bash
git add main/src/presentation/mqtt/handler/action/ main/include/presentation/mqtt/handler/action/ main/src/presentation/mqtt/event/on_message.c
git commit -m "refactor: extract MQTT action handler's JSON decoding into a dedicated dto.c, matching BLE handlers' layout"
```

### Task 8: Extract `ota`'s DTO into its own subdirectory

**Files:**
- Delete: `main/src/presentation/mqtt/handler/ota.c`
- Delete: `main/include/presentation/mqtt/handler/ota.h`
- Create: `main/src/presentation/mqtt/handler/ota/handler.c`
- Create: `main/src/presentation/mqtt/handler/ota/dto.c`
- Create: `main/include/presentation/mqtt/handler/ota/handler.h`
- Create: `main/include/presentation/mqtt/handler/ota/dto.h`
- Modify: `main/src/presentation/mqtt/event/on_message.c`

**Interfaces:**
- Produces: `dom_models_error_t pres_mqtt_handler_ota_dto_decode(const char* data, int data_len, dom_models_update_info_t* out)`.

- [ ] **Step 1: Write the DTO header**

Create `main/include/presentation/mqtt/handler/ota/dto.h`:
```c
#ifndef PRESENTATION_MQTT_HANDLER_OTA_DTO_H
#define PRESENTATION_MQTT_HANDLER_OTA_DTO_H

#include "domain/models/error.h"
#include "domain/models/update.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parses the /sub/<device_id>/ota payload into a fully-populated
   dom_models_update_info_t. Returns DOMAIN_MODELS_ERROR_BAD_ARGUMENT if
   data is empty, not valid JSON, or missing/mistyped any of
   firmware_url/firmware_size/firmware_checksum - unlike action's DTO,
   there's no valid partial-decode case for OTA (see ota/impl.c's update_impl,
   which requires all three fields to attempt a download). */
dom_models_error_t pres_mqtt_handler_ota_dto_decode(
    const char*                data,
    int                        data_len,
    dom_models_update_info_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_OTA_DTO_H */
```

- [ ] **Step 2: Write the DTO implementation**

Create `main/src/presentation/mqtt/handler/ota/dto.c`:
```c
#include "presentation/mqtt/handler/ota/dto.h"

#include <string.h>

#include "cJSON.h"

dom_models_error_t pres_mqtt_handler_ota_dto_decode(
    const char*                data,
    int                        data_len,
    dom_models_update_info_t* out
) {
    if (!data || data_len <= 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* json = cJSON_ParseWithLength(data, (size_t)data_len);
    if (!json) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* url_item      = cJSON_GetObjectItemCaseSensitive(json, "firmware_url");
    cJSON* size_item     = cJSON_GetObjectItemCaseSensitive(json, "firmware_size");
    cJSON* checksum_item = cJSON_GetObjectItemCaseSensitive(json, "firmware_checksum");

    if (!cJSON_IsString(url_item) || !cJSON_IsNumber(size_item) || !cJSON_IsString(checksum_item)) {
        cJSON_Delete(json);
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->firmware_url, url_item->valuestring, sizeof(out->firmware_url) - 1);
    out->firmware_size = (size_t)size_item->valuedouble;
    strncpy(out->firmware_checksum, checksum_item->valuestring, sizeof(out->firmware_checksum) - 1);

    cJSON_Delete(json);

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 3: Write the new handler header**

Create `main/include/presentation/mqtt/handler/ota/handler.h`:
```c
#ifndef PRESENTATION_MQTT_HANDLER_OTA_HANDLER_H
#define PRESENTATION_MQTT_HANDLER_OTA_HANDLER_H

#include "presentation/mqtt/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void pres_mqtt_handler_ota(pres_mqtt_context_t* ctx, const char* data, int data_len);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_OTA_HANDLER_H */
```

- [ ] **Step 4: Write the new handler implementation**

Create `main/src/presentation/mqtt/handler/ota/handler.c`:
```c
#include "presentation/mqtt/handler/ota/handler.h"

#include "domain/models/error.h"
#include "domain/models/update.h"
#include "presentation/mqtt/handler/ota/dto.h"

#define BASE_TAG "pres_mqtt_ota"

void pres_mqtt_handler_ota(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->info(ctx->logger, tag, "Received OTA update trigger via MQTT");

    dom_models_update_info_t update_info;
    dom_models_error_t       err = pres_mqtt_handler_ota_dto_decode(data, data_len, &update_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, tag, "Failed to parse OTA payload: %s (%d)", dom_models_error_str(err), (int)err);
        return;
    }

    err = ctx->ota->update(ctx->ota, &update_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        /* No error log on failure here - the usecase (ota's update_impl)
           already logs the failure; see Phase 6. */
        return;
    }

    ctx->logger->info(ctx->logger, tag, "OTA update requested for URL: %s", update_info.firmware_url);
}
```

- [ ] **Step 5: Delete the old flat files**

```bash
git rm main/src/presentation/mqtt/handler/ota.c main/include/presentation/mqtt/handler/ota.h
```

- [ ] **Step 6: Update the include path in `on_message.c`**

In `main/src/presentation/mqtt/event/on_message.c`, change:
```c
#include "presentation/mqtt/handler/ota.h"
```
to:
```c
#include "presentation/mqtt/handler/ota/handler.h"
```

- [ ] **Step 7: Build**

```bash
idf.py build
```

- [ ] **Step 8: Commit**

```bash
git add main/src/presentation/mqtt/handler/ota/ main/include/presentation/mqtt/handler/ota/ main/src/presentation/mqtt/event/on_message.c
git commit -m "refactor: extract MQTT ota handler's JSON decoding into a dedicated dto.c, matching BLE handlers' layout"
```

### Phase 4 checkpoint (hardware-in-loop)

- [ ] Flash to the real device. Re-run `docs/agent_test/v1.0.0-dev.1/scenario/05-action-dispatch.md`'s full `ACT-01`–`ACT-08` matrix (positive restart, omitted delay_ms, missing execution_id, missing action field, unknown action) and `scenario/06-ota-update.md`'s `OTA-01`–`OTA-06` (successful cycle, checksum-mismatch rejection, unreachable-URL handling) — these are exactly the message-parsing paths this phase touched, and both handlers' error-response behavior for malformed input must be byte-identical to before (verified against the exact `_set`-flag / decode-failure logic transplanted from the original inline cJSON code).

---

## Phase 5 — MQTT buffer ownership fix

### Task 9: Move `on_message.c`'s stack-local topic buffers into `pres_mqtt_context_t`

**Files:**
- Modify: `main/include/presentation/mqtt/context.h`
- Modify: `main/src/presentation/mqtt/context.c`
- Modify: `main/src/presentation/mqtt/event/on_message.c`

**Interfaces:**
- Produces: new struct-owned fields on `pres_mqtt_context_t` — `char topic_scratch[...]`, `char registration_ack_topic[...]`, `char ota_topic[...]`, `char action_topic[...]`. The "expected" topic strings are now precomputed once (in `_init`, since they depend only on `device_id_str` which is already known by then) rather than rebuilt with 3x `snprintf` on every single incoming MQTT message.

- [ ] **Step 1: Add the buffer fields to `context.h`**

In `main/include/presentation/mqtt/context.h`, add to the struct (matching BLE's `types.h` pattern of named `_MAX_LEN` constants + a comment explaining why):
```c
#define PRES_MQTT_CONTEXT_TOPIC_MAX_LEN 128

typedef struct {
    dom_contracts_logger_leveled_t*              logger;
    dom_contracts_repository_preloaded_t*        preloaded_repository;
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks;
    dom_usecases_internal_ota_t*                 ota;
    char                                         device_id_str[37];
    bool                                         registered;
    /* Struct-owned (not stack-local in on_message.c's callback, which runs
       on esp-mqtt's internal event task) - same rationale as
       presentation/ble/handler/settings/types.h's comment: a stack-local
       buffer in a callback on a task with limited stack already caused a
       real crash this session (see docs/agent_test/v1.0.0-dev.1/scenario/09-known-gaps-summary.md,
       bugs #6/#7); this fixes the one remaining presentation handler that
       still had the same pattern before it had a chance to crash the
       same way. */
    char topic_scratch[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
    char registration_ack_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
    char ota_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
    char action_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
} pres_mqtt_context_t;
```

- [ ] **Step 2: Precompute the three expected-topic strings in `_init`**

In `main/src/presentation/mqtt/context.c`, add `#include <stdio.h>` to the includes, and in `pres_mqtt_context_init`, right after the `self->registered` idempotency check (before the `esp_mqtt_client_register_event` call), add:
```c
    int written = snprintf(self->registration_ack_topic, sizeof(self->registration_ack_topic), "/sub/%s/registration_ack", self->device_id_str);
    if (written <= 0 || (size_t)written >= sizeof(self->registration_ack_topic)) {
        self->logger->error(self->logger, tag, "Failed to build registration_ack topic string");
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    written = snprintf(self->ota_topic, sizeof(self->ota_topic), "/sub/%s/ota", self->device_id_str);
    if (written <= 0 || (size_t)written >= sizeof(self->ota_topic)) {
        self->logger->error(self->logger, tag, "Failed to build ota topic string");
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    written = snprintf(self->action_topic, sizeof(self->action_topic), "/sub/%s/action", self->device_id_str);
    if (written <= 0 || (size_t)written >= sizeof(self->action_topic)) {
        self->logger->error(self->logger, tag, "Failed to build action topic string");
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

```

- [ ] **Step 3: Rewrite `on_message.c` to use the precomputed context fields**

Replace the full content of `main/src/presentation/mqtt/event/on_message.c`:
```c
#include "presentation/mqtt/event/on_message.h"

#include <string.h>

#include "presentation/mqtt/handler/action/handler.h"
#include "presentation/mqtt/handler/ota/handler.h"
#include "presentation/mqtt/handler/registration_ack.h"

#define BASE_TAG "pres_mqtt_on_message"

void pres_mqtt_event_on_message(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    const char* tag = BASE_TAG "/handle";

    if (event->topic_len <= 0 || event->data_len < 0) {
        return;
    }

    if ((size_t)event->topic_len >= sizeof(ctx->topic_scratch)) {
        ctx->logger->error(ctx->logger, tag, "Topic too long");
        return;
    }
    memcpy(ctx->topic_scratch, event->topic, (size_t)event->topic_len);
    ctx->topic_scratch[event->topic_len] = '\0';

    if (strcmp(ctx->topic_scratch, ctx->registration_ack_topic) == 0) {
        pres_mqtt_handler_registration_ack(ctx, event->data, event->data_len);
    } else if (strcmp(ctx->topic_scratch, ctx->ota_topic) == 0) {
        pres_mqtt_handler_ota(ctx, event->data, event->data_len);
    } else if (strcmp(ctx->topic_scratch, ctx->action_topic) == 0) {
        pres_mqtt_handler_action(ctx, event->data, event->data_len);
    } else {
        ctx->logger->debug(ctx->logger, tag, "Unhandled topic: %s", ctx->topic_scratch);
    }
}
```
`registration_ack` is not restructured into a subdirectory by this plan (Phase 4 only touched `action`/`ota` — `registration_ack` has no payload parsing, so it needs no `dto.c` and stays at its current flat path), which is why its include above is unchanged from the original file while `action`/`ota`'s point at their new `handler/` subdirectory paths.

- [ ] **Step 4: Build**

```bash
idf.py build
```

- [ ] **Step 5: Commit**

```bash
git add main/include/presentation/mqtt/context.h main/src/presentation/mqtt/context.c main/src/presentation/mqtt/event/on_message.c
git commit -m "refactor: move MQTT on_message's stack-local topic buffers into context-owned storage"
```

### Phase 5 checkpoint (hardware-in-loop)

- [ ] Flash to the real device. Re-run `docs/agent_test/v1.0.0-dev.1/scenario/03-mqtt-registration-status.md`'s `REG-01`–`REG-04` (registration, ack receipt, no-matching-firmware negative case) — these exercise topic matching on all three subscribed topics through the new precomputed-topic path.

---

## Phase 6 — Strip redundant error logs

Task 7 and Task 8 already removed the redundant `logger->error` calls in `action`'s and `ota`'s handlers as part of rewriting those files (noted inline in each task above — re-adding them separately here would just be undoing work already done). This phase is the confirmation pass plus checking there's nothing else the audit didn't already cover.

### Task 10: Confirm no redundant error-logging remains in MQTT presentation handlers

**Files:**
- Verify only (no further edits expected): `main/src/presentation/mqtt/handler/action/handler.c`, `main/src/presentation/mqtt/handler/ota/handler.c`, `main/src/presentation/mqtt/handler/registration_ack.c`, `main/src/presentation/mqtt/event/on_connect.c`, `main/src/presentation/mqtt/event/on_disconnect.c`, `main/src/presentation/mqtt/event/on_error.c`

**Interfaces:** none — verification-only task.

- [ ] **Step 1: Grep for any remaining `logger->error` calls that immediately follow a usecase call returning a non-OK error**

```bash
grep -n "logger->error" main/src/presentation/mqtt/handler/action/handler.c main/src/presentation/mqtt/handler/ota/handler.c main/src/presentation/mqtt/handler/registration_ack.c
```
Expected: `action/handler.c` and `ota/handler.c` each show exactly one `logger->error` call, both for a **decode** failure (`pres_mqtt_handler_action_dto_decode`/`pres_mqtt_handler_ota_dto_decode` returning `DOMAIN_MODELS_ERROR_BAD_ARGUMENT`) — this is presentation-original information (the usecase never sees malformed JSON, so there's no usecase-layer log to be redundant with) and should stay. Neither file should have an error log immediately after `ctx->messaging_callbacks->restart(...)` or `ctx->ota->update(...)` — confirm by inspection that the code paths added in Tasks 7/8 only comment-and-return on those two calls' failure, matching the design's "log once, at the usecase" rule.

- [ ] **Step 2: Confirm `on_connect.c` is a genuine exception, not a violation**

`presentation/mqtt/event/on_connect.c` logs `ctx->logger->error(...)` after each of `publish_registration`/`publish_online_status`/`subscribe_defaults` failing. These usecase calls (`app_internal_messaging_callbacks_impl.c`'s `publish_registration_impl` etc.) already log their own failure too — this is a real instance of the same double-log pattern the audit found in `ota.c`/`action.c`, just not called out by name in the original two examples. Per the "log once, at the usecase" rule, these three `ctx->logger->error(...)` calls in `on_connect.c` should be removed too, since they're the *exact* same anti-pattern applied to a third file the initial audit sampled but didn't exhaustively enumerate.

Rewrite `main/src/presentation/mqtt/event/on_connect.c`:
```c
#include "presentation/mqtt/event/on_connect.h"

#define BASE_TAG "pres_mqtt_on_connect"

void pres_mqtt_event_on_connect(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    const char* tag = BASE_TAG "/handle";

    (void)event;

    ctx->logger->info(ctx->logger, tag, "Connected to MQTT broker");

    /* No error logs on failure below - each usecase call already logs its
       own failure (see app_internal_messaging_callbacks_impl.c); re-logging
       here would double-log the same condition. */
    (void)ctx->messaging_callbacks->publish_registration(ctx->messaging_callbacks);
    (void)ctx->messaging_callbacks->publish_online_status(ctx->messaging_callbacks);
    (void)ctx->messaging_callbacks->subscribe_defaults(ctx->messaging_callbacks);
}
```
(This also folds in Phase 8's tag-convention change for this file since it's being rewritten anyway — `BASE_TAG` instead of flat `TAG`, with a per-function suffix. Phase 8's task for `on_connect.c` becomes a no-op confirmation, noted there. `#include "domain/models/error.h"` is dropped since `dom_models_error_t` is no longer referenced in this file.)

- [ ] **Step 3: Build**

```bash
idf.py build
```

- [ ] **Step 4: Commit**

```bash
git add main/src/presentation/mqtt/event/on_connect.c
git commit -m "refactor: strip redundant error logs from mqtt on_connect, matching the log-once-at-the-usecase rule"
```

---

## Phase 7 — `validate_cfg()` extraction

### Task 11: Add `validate_cfg()` helpers to the 4 BLE modules + MQTT context, replacing inline checks

**Files:**
- Create: `main/include/presentation/ble/handler/log/utils.h`
- Create: `main/src/presentation/ble/handler/log/utils.c`
- Modify: `main/src/presentation/ble/handler/log/handler.c`
- Create: `main/include/presentation/ble/handler/settings/utils.h`
- Create: `main/src/presentation/ble/handler/settings/utils.c`
- Modify: `main/src/presentation/ble/handler/settings/handler.c`
- Create: `main/include/presentation/ble/handler/wifi_manager/utils.h`
- Create: `main/src/presentation/ble/handler/wifi_manager/utils.c`
- Modify: `main/src/presentation/ble/handler/wifi_manager/handler.c`
- Create: `main/include/presentation/ble/host_utils.h`
- Create: `main/src/presentation/ble/host_utils.c`
- Modify: `main/src/presentation/ble/host.c`
- Create: `main/include/presentation/mqtt/context_utils.h`
- Create: `main/src/presentation/mqtt/context_utils.c`
- Modify: `main/src/presentation/mqtt/context.c`

**Interfaces:** each new file exposes exactly one function, matching the existing `app_internal_*_impl_validate_cfg` naming convention adapted to each module's own prefix, e.g. `pres_ble_handler_log_validate_cfg(const pres_ble_handler_log_cfg_t* cfg)`.

- [ ] **Step 1: `presentation/ble/handler/log`**

Create `main/include/presentation/ble/handler/log/utils.h`:
```c
#ifndef PRESENTATION_BLE_HANDLER_LOG_UTILS_H
#define PRESENTATION_BLE_HANDLER_LOG_UTILS_H

#include "domain/models/error.h"
#include "presentation/ble/handler/log/types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_ble_handler_log_validate_cfg(const pres_ble_handler_log_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_LOG_UTILS_H */
```
Create `main/src/presentation/ble/handler/log/utils.c`:
```c
#include "presentation/ble/handler/log/utils.h"

dom_models_error_t pres_ble_handler_log_validate_cfg(const pres_ble_handler_log_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->log_forwarding || !cfg->gatt_registry || !cfg->host) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```
In `main/src/presentation/ble/handler/log/handler.c`, add `#include "presentation/ble/handler/log/utils.h"` and replace:
```c
pres_ble_handler_log_t* pres_ble_handler_log_new(const pres_ble_handler_log_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->log_forwarding || !cfg->gatt_registry || !cfg->host) {
        return NULL;
    }
```
with:
```c
pres_ble_handler_log_t* pres_ble_handler_log_new(const pres_ble_handler_log_cfg_t* cfg) {
    if (pres_ble_handler_log_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }
```

- [ ] **Step 2: `presentation/ble/handler/settings`**

Create `main/include/presentation/ble/handler/settings/utils.h`:
```c
#ifndef PRESENTATION_BLE_HANDLER_SETTINGS_UTILS_H
#define PRESENTATION_BLE_HANDLER_SETTINGS_UTILS_H

#include "domain/models/error.h"
#include "presentation/ble/handler/settings/types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_ble_handler_settings_validate_cfg(const pres_ble_handler_settings_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SETTINGS_UTILS_H */
```
Create `main/src/presentation/ble/handler/settings/utils.c`:
```c
#include "presentation/ble/handler/settings/utils.h"

dom_models_error_t pres_ble_handler_settings_validate_cfg(const pres_ble_handler_settings_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->settings || !cfg->gatt_registry) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```
In `main/src/presentation/ble/handler/settings/handler.c`, add `#include "presentation/ble/handler/settings/utils.h"` and replace:
```c
pres_ble_handler_settings_t* pres_ble_handler_settings_new(const pres_ble_handler_settings_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->settings || !cfg->gatt_registry) {
        return NULL;
    }
```
with:
```c
pres_ble_handler_settings_t* pres_ble_handler_settings_new(const pres_ble_handler_settings_cfg_t* cfg) {
    if (pres_ble_handler_settings_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }
```

- [ ] **Step 3: `presentation/ble/handler/wifi_manager`**

Create `main/include/presentation/ble/handler/wifi_manager/utils.h`:
```c
#ifndef PRESENTATION_BLE_HANDLER_WIFI_MANAGER_UTILS_H
#define PRESENTATION_BLE_HANDLER_WIFI_MANAGER_UTILS_H

#include "domain/models/error.h"
#include "presentation/ble/handler/wifi_manager/types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_ble_handler_wifi_manager_validate_cfg(const pres_ble_handler_wifi_manager_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_WIFI_MANAGER_UTILS_H */
```
Create `main/src/presentation/ble/handler/wifi_manager/utils.c`:
```c
#include "presentation/ble/handler/wifi_manager/utils.h"

dom_models_error_t pres_ble_handler_wifi_manager_validate_cfg(const pres_ble_handler_wifi_manager_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->wifi_manager || !cfg->gatt_registry) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```
In `main/src/presentation/ble/handler/wifi_manager/handler.c`, add `#include "presentation/ble/handler/wifi_manager/utils.h"` and replace:
```c
pres_ble_handler_wifi_manager_t* pres_ble_handler_wifi_manager_new(const pres_ble_handler_wifi_manager_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->wifi_manager || !cfg->gatt_registry) {
        return NULL;
    }
```
with:
```c
pres_ble_handler_wifi_manager_t* pres_ble_handler_wifi_manager_new(const pres_ble_handler_wifi_manager_cfg_t* cfg) {
    if (pres_ble_handler_wifi_manager_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }
```

- [ ] **Step 4: `presentation/ble/host`**

Create `main/include/presentation/ble/host_utils.h`:
```c
#ifndef PRESENTATION_BLE_HOST_UTILS_H
#define PRESENTATION_BLE_HOST_UTILS_H

#include "domain/models/error.h"
#include "presentation/ble/host.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_ble_host_validate_cfg(const pres_ble_host_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HOST_UTILS_H */
```
Create `main/src/presentation/ble/host_utils.c`:
```c
#include "presentation/ble/host_utils.h"

dom_models_error_t pres_ble_host_validate_cfg(const pres_ble_host_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->gatt_registry) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```
In `main/src/presentation/ble/host.c`, add `#include "presentation/ble/host_utils.h"` and replace:
```c
pres_ble_host_t* pres_ble_host_new(const pres_ble_host_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->gatt_registry) {
        return NULL;
    }
```
with:
```c
pres_ble_host_t* pres_ble_host_new(const pres_ble_host_cfg_t* cfg) {
    if (pres_ble_host_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }
```

- [ ] **Step 5: `presentation/mqtt/context`**

Create `main/include/presentation/mqtt/context_utils.h`:
```c
#ifndef PRESENTATION_MQTT_CONTEXT_UTILS_H
#define PRESENTATION_MQTT_CONTEXT_UTILS_H

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/models/error.h"
#include "domain/usecases/internal/messaging_callbacks.h"
#include "domain/usecases/internal/ota.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_mqtt_context_validate_cfg(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_ota_t*                 ota
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_CONTEXT_UTILS_H */
```
(Signature takes the four raw pointers rather than a `cfg` struct, since `pres_mqtt_context_new` — unlike every other module in this codebase — takes four separate parameters instead of one `cfg` struct. That inconsistency is out of scope for this refactor per the spec's non-goals; this task only extracts the validation that already exists, it doesn't restructure the constructor's parameter list.)

Create `main/src/presentation/mqtt/context_utils.c`:
```c
#include "presentation/mqtt/context_utils.h"

dom_models_error_t pres_mqtt_context_validate_cfg(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_ota_t*                 ota
) {
    if (!logger || !preloaded_repository || !messaging_callbacks || !ota) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```
In `main/src/presentation/mqtt/context.c`, add `#include "presentation/mqtt/context_utils.h"` and replace:
```c
    if (!logger || !preloaded_repository || !messaging_callbacks || !ota) {
        return NULL;
    }
```
with:
```c
    if (pres_mqtt_context_validate_cfg(logger, preloaded_repository, messaging_callbacks, ota) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }
```

- [ ] **Step 6: Build**

```bash
idf.py build
```

- [ ] **Step 7: Commit**

```bash
git add main/include/presentation/ble/handler/log/utils.h main/src/presentation/ble/handler/log/utils.c main/src/presentation/ble/handler/log/handler.c \
        main/include/presentation/ble/handler/settings/utils.h main/src/presentation/ble/handler/settings/utils.c main/src/presentation/ble/handler/settings/handler.c \
        main/include/presentation/ble/handler/wifi_manager/utils.h main/src/presentation/ble/handler/wifi_manager/utils.c main/src/presentation/ble/handler/wifi_manager/handler.c \
        main/include/presentation/ble/host_utils.h main/src/presentation/ble/host_utils.c main/src/presentation/ble/host.c \
        main/include/presentation/mqtt/context_utils.h main/src/presentation/mqtt/context_utils.c main/src/presentation/mqtt/context.c
git commit -m "refactor: extract validate_cfg() helpers for the 4 BLE modules and mqtt context, matching the application layer's existing convention"
```

---

## Phase 8 — Logging tag convention normalization

`BASE_TAG` + per-function suffix everywhere. `on_connect.c` was already converted in Phase 6 (Task 10, Step 2) since it was being rewritten anyway — its entry below is a no-op confirmation only.

### Task 12: Rename `TAG_PATH`→`BASE_TAG` in `ble/host.c`, and flat `TAG`→`BASE_TAG`+suffix across every MQTT/composition file that still uses it

**Files:**
- Modify: `main/src/presentation/ble/host.c`
- Modify: `main/src/presentation/mqtt/event/on_disconnect.c`
- Modify: `main/src/presentation/mqtt/event/on_error.c`
- Modify: `main/src/presentation/mqtt/handler/registration_ack.c`
- Modify: `main/src/composition/main/launcher.c`
- Modify: `main/src/composition/test_seed/seed.c`
- Verify only: `main/src/presentation/mqtt/event/on_connect.c` (already done in Phase 6), `main/src/presentation/mqtt/handler/action/handler.c` and `main/src/presentation/mqtt/handler/ota/handler.c` (already used `BASE_TAG` from the start when created in Phase 4 — confirm, don't re-edit), `main/src/presentation/mqtt/event/on_message.c` (already used `BASE_TAG` when rewritten in Phase 5 — confirm, don't re-edit)

**Interfaces:** none — pure rename, no signature changes.

- [ ] **Step 1: `ble/host.c` — rename the macro only, value unchanged**

In `main/src/presentation/ble/host.c`, change `#define TAG_PATH "ble/host"` to `#define BASE_TAG "ble/host"`, and every use of `TAG_PATH` in the file (there are several, e.g. `const char* tag = TAG_PATH "/start";`) to `BASE_TAG` — mechanical find-and-replace of the identifier `TAG_PATH` → `BASE_TAG` throughout this one file only.

- [ ] **Step 2: `on_disconnect.c`**

Replace `main/src/presentation/mqtt/event/on_disconnect.c`:
```c
#include "presentation/mqtt/event/on_disconnect.h"

#define BASE_TAG "pres_mqtt_on_disconnect"

void pres_mqtt_event_on_disconnect(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    const char* tag = BASE_TAG "/handle";

    (void)event;

    ctx->logger->warn(ctx->logger, tag, "Disconnected from MQTT broker");
}
```

- [ ] **Step 3: `on_error.c`**

Replace `main/src/presentation/mqtt/event/on_error.c`:
```c
#include "presentation/mqtt/event/on_error.h"

#define BASE_TAG "pres_mqtt_on_error"

void pres_mqtt_event_on_error(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->error(ctx->logger, tag, "MQTT error event type: %d", (int)event->error_handle->error_type);
}
```

- [ ] **Step 4: `registration_ack.c`**

Replace `main/src/presentation/mqtt/handler/registration_ack.c`:
```c
#include "presentation/mqtt/handler/registration_ack.h"

#define BASE_TAG "pres_mqtt_registration_ack"

void pres_mqtt_handler_registration_ack(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    (void)data;
    (void)data_len;

    ctx->logger->info(ctx->logger, tag, "Received registration ack via MQTT");
}
```

- [ ] **Step 5: `composition/main/launcher.c`**

This file uses ESP-IDF's own `ESP_LOGE`/`ESP_LOGI` macros (not the domain logger contract — it runs before the domain logger exists), so its `TAG` is a different kind of tag (ESP-IDF's own logging component tag, one per file by ESP-IDF convention, not per-function). Leave `#define TAG "cmp_main_launcher"` as-is — renaming this would fight ESP-IDF's own `esp_log.h` convention, which is intentionally one static tag per file, not per-function. No change to this file.

- [ ] **Step 6: `composition/test_seed/seed.c` — confirmed no change needed**

Verified: this file uses ESP-IDF's own `ESP_LOGE`/`ESP_LOGI`/`ESP_LOGW` macros with `#define TAG "test_seed"` (line 18), the same exception as `launcher.c` in Step 5 — it runs before the domain logger exists, so it's ESP-IDF's own per-file component tag, not the domain logger contract's per-function tag. No change to this file. (No need to re-run the grep from Step 5's pattern — already confirmed via `grep -n "TAG\|logger->\|ESP_LOG" main/src/composition/test_seed/seed.c` during plan-writing, which showed only `ESP_LOGE`/`ESP_LOGI`/`ESP_LOGW` call sites, e.g. line 53 `ESP_LOGE(TAG, "Failed to set %s: %s (%d)", ...)` and line 101 `ESP_LOGW(TAG, "Seed complete. Reflash the normal firmware now...")`.)

- [ ] **Step 7: Confirm the three already-converted MQTT files need no further edits**

```bash
grep -n "BASE_TAG" main/src/presentation/mqtt/event/on_connect.c main/src/presentation/mqtt/event/on_message.c main/src/presentation/mqtt/handler/action/handler.c main/src/presentation/mqtt/handler/ota/handler.c
```
Expected: all four already define and use `BASE_TAG` (set in Phase 6 Task 10 Step 2, and Phase 4/5's Tasks 7/8/9) — no edits needed here.

- [ ] **Step 8: Final sweep — confirm no flat `TAG` remains anywhere under `presentation/mqtt` or `composition` that talks to the domain logger contract**

```bash
grep -rn '#define TAG "' main/src/presentation/mqtt main/src/composition
```
Expected: only `composition/main/launcher.c` (and possibly `composition/test_seed/seed.c`, per Step 6's finding) remain, both using ESP-IDF's own `ESP_LOGx` macros rather than the domain logger contract — confirmed as the intentional exception, not a miss.

- [ ] **Step 9: Build**

```bash
idf.py build
```

- [ ] **Step 10: Commit**

```bash
git add main/src/presentation/ble/host.c main/src/presentation/mqtt/event/on_disconnect.c main/src/presentation/mqtt/event/on_error.c main/src/presentation/mqtt/handler/registration_ack.c
git commit -m "refactor: normalize logging tags to BASE_TAG + per-function suffix across remaining MQTT/BLE files"
```

### Phase 8 checkpoint — final full regression pass (hardware-in-loop)

- [ ] Flash to the real device and re-run the **complete** `docs/agent_test/v1.0.0-dev.1/checklist.md` — all 42 original WiFi/MQTT/OTA cases plus the 11 BLE cases from `scenario/10-ble-gatt-services.md` — to confirm zero regressions from a refactor that was explicitly scoped as "no behavior change." Update the checklist's checkboxes and, if anything unexpectedly deviates, add it to `scenario/09-known-gaps-summary.md` following the same format as the existing entries (bug description, root cause, fix, verification) rather than silently reverting.
- [ ] Run `git log --oneline` over this plan's 12 commits and confirm the sequence matches the phase order above — this refactor was designed to be revertible phase-by-phase if a checkpoint fails, so commit granularity matters as a safety net, not just as a formality.
