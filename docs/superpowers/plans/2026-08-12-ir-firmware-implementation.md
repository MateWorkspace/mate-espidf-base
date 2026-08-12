# IR Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the hardware-validated `infrared` ESP-IDF component from `smart-IR-embedded` into `mate-espidf-base`, wire it through this repo's layered architecture as a new `composition/infrared` build variant, and rename the `ir_capture`/`ir_transmit`/`ir_transmit_ack` MQTT topics to `/ir/rx`, `/ir/tx`, `/ir/tx_ack` on both `mate-espidf-base` and `mate-things`.

**Architecture:** `domain/contracts/device/ir.h` (peripheral contract) → `infrastructure/device/ir/esp_impl` (wraps the ported component) → `domain/usecases/internal/infrared.h` → `application/internal/infrared/impl` (MQTT-facing IR usecase: subscribe to `ir/tx`, publish `ir/rx` from the RX callback, publish `ir/tx_ack` after a transmit attempt) → `presentation/mqtt` (routes `ir/tx` messages in, replies `FAILED` if IR isn't wired on the active composition) → `composition/infrared` (a full parallel composition to `composition/main`, only build variant that actually subscribes to `ir/tx`). `mate-things` gets a matching, payload-preserving topic rename plus a topic-parser fix for the now-nested `ir/rx` suffix.

**Tech Stack:** ESP-IDF v6.0.2 (C11, FreeRTOS, RMT TX, GPIO ISR, `esp_timer`, `cJSON`), Go 1.x backend (`paho.mqtt.golang`).

## Global Constraints

- No functional changes to the ported `infrared` component's RX/TX internals — same lifecycle, same constants, same ESP-IDF-native (`esp_err_t`) boundary.
- MQTT payload shapes are unchanged everywhere — only topic string literals move. `raw_data` is an alternating mark/space `int32` microsecond array, always starting with a mark.
- RX/TX GPIOs are hardcoded `#define` constants in the infrastructure layer, not Kconfig.
- `composition/main` never subscribes to `ir/tx`; only `composition/infrared` does. `composition/main` stays the active (uncommented) launcher in `main.c`; `composition/infrared` is added commented-out.
- Every `domain/contracts/messaging/def_pub.h` / `def_sub.h` method must be implemented by both `mqtt_impl.c` and `stub_impl.c` (existing repo convention — no partial contract implementations).
- Format touched C/C++ files with `clang-format` (repo's `.clang-format`); `git diff --check` must be clean. Run `idf.py reconfigure && idf.py build` after any file add/move/remove.
- Full design rationale lives in `docs/superpowers/specs/2026-08-11-ir-firmware-implementation-design.md` — consult it if a task step seems to need justification beyond what's written here.

---

## Task 1: Port the `infrared` ESP-IDF component

**Files:**
- Create: `mate-espidf-base/components/infrared/CMakeLists.txt`
- Create: `mate-espidf-base/components/infrared/include/infrared.h`
- Create: `mate-espidf-base/components/infrared/include/infrared_types.h`
- Create: `mate-espidf-base/components/infrared/include/infrared_rx.h`
- Create: `mate-espidf-base/components/infrared/include/infrared_tx.h`
- Create: `mate-espidf-base/components/infrared/src/infrared.c`
- Create: `mate-espidf-base/components/infrared/src/infrared_internal.h`
- Create: `mate-espidf-base/components/infrared/src/infrared_rx.c`
- Create: `mate-espidf-base/components/infrared/src/infrared_tx.c`
- Create: `mate-espidf-base/components/infrared/AGENTS.md`

**Interfaces:**
- Produces (consumed by Task 3): `infrared_handle_t`, `infrared_cfg_t`, `infrared_duration_t{bool level; uint32_t duration_us;}`, `infrared_rx_frame_t{durations, duration_count, overflowed}`, `infrared_receive_cb_t`, `INFRARED_CFG_DEFAULT()`, `infrared_new/_init/_deinit/_delete/_transmit` — all returning/taking `esp_err_t`/ESP-IDF types, exactly as in `smart-IR-embedded/components/infrared`.

- [ ] **Step 1: Copy the component source and headers verbatim**

```bash
mkdir -p /home/dodol/Repositories/mate/mate-espidf-base/components/infrared/include
mkdir -p /home/dodol/Repositories/mate/mate-espidf-base/components/infrared/src
cp /home/dodol/Repositories/mate/smart-IR-embedded/components/infrared/CMakeLists.txt \
   /home/dodol/Repositories/mate/mate-espidf-base/components/infrared/CMakeLists.txt
cp /home/dodol/Repositories/mate/smart-IR-embedded/components/infrared/include/infrared.h \
   /home/dodol/Repositories/mate/smart-IR-embedded/components/infrared/include/infrared_types.h \
   /home/dodol/Repositories/mate/smart-IR-embedded/components/infrared/include/infrared_rx.h \
   /home/dodol/Repositories/mate/smart-IR-embedded/components/infrared/include/infrared_tx.h \
   /home/dodol/Repositories/mate/mate-espidf-base/components/infrared/include/
cp /home/dodol/Repositories/mate/smart-IR-embedded/components/infrared/src/infrared.c \
   /home/dodol/Repositories/mate/smart-IR-embedded/components/infrared/src/infrared_internal.h \
   /home/dodol/Repositories/mate/smart-IR-embedded/components/infrared/src/infrared_rx.c \
   /home/dodol/Repositories/mate/smart-IR-embedded/components/infrared/src/infrared_tx.c \
   /home/dodol/Repositories/mate/mate-espidf-base/components/infrared/src/
```

No content edits to any of these 9 files — they are byte-for-byte the same as `smart-IR-embedded`'s (RX: GPIO ISR + `esp_timer` 20000us idle-timeout finalization, 512 max durations, 4-deep RX queue; TX: RMT copy encoder, 38kHz/33% duty carrier; `_new/_init/_deinit/_delete` lifecycle).

- [ ] **Step 2: Write the adapted component `AGENTS.md`**

```markdown
# Infrared Component Guide

This component hides infrared receive and transmit mechanics from the rest
of the firmware. It is an ESP-IDF component under `components/infrared`,
independent of the Clean Architecture layers in `main/` — `main/` depends on
its public API (`infrared.h`, `infrared_types.h`) only, never on GPIO ISR
details, timers, queues, or the RMT TX backend directly.

Ported from `smart-IR-embedded/components/infrared` (already
hardware-validated there) with no functional changes. `mate-espidf-base`
wraps it at `main/src/infrastructure/device/ir/esp_impl.c`, which is the
only place in `main/` that includes this component's headers.

## Public API Shape

Centered on `infrared.h` + `infrared_types.h`. Config (`infrared_cfg_t`):
`rx_gpio`, `tx_gpio`, `enable_rx`, `enable_tx`, `on_receive`, `user_ctx`,
`rx_task_stack_size`, `rx_task_priority`, `rx_task_poll_ms`,
`rx_queue_depth`. Start from `INFRARED_CFG_DEFAULT()` and override only the
fields needed.

Validation: `enable_rx` requires a valid `rx_gpio` and non-NULL
`on_receive`; `enable_tx` requires a valid output `tx_gpio`; at least one of
`enable_rx`/`enable_tx` must be true.

Lifecycle: `infrared_new(cfg)` allocates and copies config only.
`infrared_init(handle)` starts runtime resources for the enabled
direction(s). `infrared_deinit(handle)` stops and releases whatever was
successfully initialized (idempotent, safe on partial init).
`infrared_delete(handle)` frees memory only — it does not call
`infrared_deinit()`; callers must deinit first.

`on_receive` runs on the RX worker task, never from the GPIO ISR or timer
callback. It receives a read-only `infrared_rx_frame_t*`; callers must copy
data they need to keep past the callback's return.

## File Ownership

- `include/infrared.h` — public facade lifecycle + transmit declarations.
- `include/infrared_types.h` — config, default-config macro, frame,
  duration, callback typedefs, handle forward-declaration.
- `include/infrared_rx.h` / `include/infrared_tx.h` — internal RX/TX
  declarations, exposed for facade composition.
- `src/infrared.c` — facade allocation, validation, lifecycle
  orchestration, transmit forwarding.
- `src/infrared_rx.c` — RX GPIO setup, ISR, idle timer, frame buffering,
  queueing, worker task, RX cleanup.
- `src/infrared_tx.c` — RMT TX channel/encoder setup, transmit validation
  and execution, TX cleanup.
- `src/infrared_internal.h` — the private `infrared_handle_s` struct
  definition, not exposed outside the component.

## Receive Flow

1. RX GPIO configured as input, `GPIO_INTR_ANYEDGE`.
2. Each edge ISR reads pin level + `esp_timer_get_time()`, stores the
   elapsed duration since the previous edge into the current frame buffer,
   (re)starts a 20000us idle timer. No allocation, no callbacks, no
   protocol parsing inside the ISR.
3. When the idle timer fires with no further edge, the frame is finalized
   under a critical section and pushed onto a 4-deep FreeRTOS queue.
4. The RX worker task drains the queue and calls `on_receive(frame,
   user_ctx)`.

Overflow behavior: a frame exceeding 512 durations is marked overflowed and
dropped on finalization; a full queue drops the finalized frame. Both cases
are silently counted internally, not surfaced through the public API today.

## Transmit Flow

`infrared_transmit` validates TX is enabled, converts the duration sequence
to RMT symbol words (splitting any single duration longer than the RMT
hardware's 32767us max into multiple parts), and transmits through the
configured TX GPIO via a copy encoder with a 38kHz/33%-duty carrier applied
independently of the symbol data. Blocks (under a mutex) until
`rmt_tx_wait_all_done` or a 10s timeout.

## ESP-IDF Dependencies

`driver/gpio`, `esp_timer`, `freertos`, `esp_driver_rmt` (RMT TX). Declared
in this component's own `CMakeLists.txt` `REQUIRES`; `main/CMakeLists.txt`
pulls this component in via `PRIV_REQUIRES infrared`.
```

- [ ] **Step 3: Commit**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
git add components/infrared
git commit -m "$(cat <<'EOF'
feat: (ir) port infrared ESP-IDF component from smart-IR-embedded

GPIO-ISR + esp_timer idle-timeout RX and RMT-based TX, ported verbatim
(no functional changes). Not yet wired into any composition - that
happens in later commits.
EOF
)"
```

No build-verification step here: this component isn't yet required by
`main`'s `PRIV_REQUIRES`, so ESP-IDF won't compile it until Task 3 adds
that dependency edge. Verified there.

---

## Task 2: Extend `def_pub`/`def_sub` messaging contracts for IR topics

**Files:**
- Modify: `main/include/domain/contracts/messaging/def_pub.h`
- Modify: `main/include/domain/contracts/messaging/def_sub.h`
- Modify: `main/include/infrastructure/messaging/def_pub/mqtt_impl_utils.h`
- Modify: `main/src/infrastructure/messaging/def_pub/mqtt_impl_utils.c`
- Modify: `main/src/infrastructure/messaging/def_pub/mqtt_impl.c`
- Modify: `main/src/infrastructure/messaging/def_sub/mqtt_impl.c`
- Modify: `main/include/infrastructure/messaging/def_pub/stub_impl_types.h`
- Modify: `main/include/infrastructure/messaging/def_pub/stub_impl_utils.h`
- Modify: `main/src/infrastructure/messaging/def_pub/stub_impl_utils.c`
- Modify: `main/src/infrastructure/messaging/def_pub/stub_impl.c`
- Modify: `main/include/infrastructure/messaging/def_sub/stub_impl_types.h`
- Modify: `main/include/infrastructure/messaging/def_sub/stub_impl_utils.h`
- Modify: `main/src/infrastructure/messaging/def_sub/stub_impl_utils.c`
- Modify: `main/src/infrastructure/messaging/def_sub/stub_impl.c`

**Interfaces:**
- Produces (consumed by Task 4): `dom_contracts_messaging_def_pub_t.ir_capture(self, device_id, const int32_t* raw_data, size_t raw_data_count)`, `dom_contracts_messaging_def_pub_t.ir_transmit_ack(self, device_id, execution_id, status, message)`, `dom_contracts_messaging_def_sub_t.ir_tx(device_id, self)`.

- [ ] **Step 1: Add the new contract members**

In `main/include/domain/contracts/messaging/def_pub.h`, add `#include <stdint.h>` to the includes, and add these two members to `struct dom_contracts_messaging_def_pub_t` (after `action_ack`, before `telemetry`):

```c
    dom_models_error_t (*ir_capture)(
        dom_contracts_messaging_def_pub_t* self,
        const char*                        device_id,
        const int32_t*                     raw_data,
        size_t                              raw_data_count
    );
    dom_models_error_t (*ir_transmit_ack)(
        dom_contracts_messaging_def_pub_t* self,
        const char*                        device_id,
        const char*                        execution_id,
        const char*                        status,
        const char*                        message
    );
```

In `main/include/domain/contracts/messaging/def_sub.h`, add this member to `struct dom_contracts_messaging_def_sub_t` (after `config`):

```c
    dom_models_error_t (*ir_tx)(
        const char*                        device_id,
        dom_contracts_messaging_def_sub_t* self
    );
```

- [ ] **Step 2: Run a build to confirm the contract header changes alone don't break anything yet**

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py build
```
Expected: still succeeds — adding struct members doesn't break existing partial-initializer assignments in C (unset function pointers become implicit `NULL` via each impl's `calloc`-based `_new`, and `dom_contracts_messaging_def_pub_new`/`def_sub_new` already `calloc` the struct).

- [ ] **Step 3: Add JSON builder helpers to `def_pub`'s MQTT impl utils**

In `main/include/infrastructure/messaging/def_pub/mqtt_impl_utils.h`, add `#include <stdint.h>` and these two declarations (after `build_action_ack_json`):

```c
char* inf_messaging_def_pub_mqtt_impl_build_ir_capture_json(
    const int32_t* raw_data,
    size_t         raw_data_count
);

char* inf_messaging_def_pub_mqtt_impl_build_ir_transmit_ack_json(
    const char* execution_id,
    const char* status,
    const char* message
);
```

In `main/src/infrastructure/messaging/def_pub/mqtt_impl_utils.c`, add the implementations (after `inf_messaging_def_pub_mqtt_impl_build_action_ack_json`'s body, before `inf_messaging_def_pub_mqtt_impl_build_telemetry_json`):

```c
char* inf_messaging_def_pub_mqtt_impl_build_ir_capture_json(
    const int32_t* raw_data,
    size_t         raw_data_count
) {
    if (!raw_data || raw_data_count == 0) {
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    cJSON* raw_data_array = cJSON_AddArrayToObject(root, "raw_data");
    if (!raw_data_array) {
        cJSON_Delete(root);
        return NULL;
    }

    for (size_t i = 0; i < raw_data_count; i++) {
        cJSON* item = cJSON_CreateNumber((double)raw_data[i]);
        if (!item || !cJSON_AddItemToArray(raw_data_array, item)) {
            cJSON_Delete(item);
            cJSON_Delete(root);
            return NULL;
        }
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

char* inf_messaging_def_pub_mqtt_impl_build_ir_transmit_ack_json(
    const char* execution_id,
    const char* status,
    const char* message
) {
    if (!cstr_available(execution_id) || !cstr_available(status)) {
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "execution_id", execution_id) ||
        !cJSON_AddStringToObject(root, "status", status)) {
        cJSON_Delete(root);
        return NULL;
    }

    if (cstr_available(message) && !cJSON_AddStringToObject(root, "message", message)) {
        cJSON_Delete(root);
        return NULL;
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}
```
(`cstr_available` is the existing static helper already defined at the bottom of this same `.c` file — no new helper needed.)

- [ ] **Step 4: Wire the two new `def_pub` contract methods in `mqtt_impl.c`**

In `main/src/infrastructure/messaging/def_pub/mqtt_impl.c`, add two static prototypes near the top (after `action_ack_impl`'s prototype):

```c
static dom_models_error_t ir_capture_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const int32_t*                     raw_data,
    size_t                              raw_data_count
);
static dom_models_error_t ir_transmit_ack_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        execution_id,
    const char*                        status,
    const char*                        message
);
```

Assign them in `inf_messaging_def_pub_mqtt_impl_new` (after `self->action_ack = action_ack_impl;`):

```c
    self->ir_capture      = ir_capture_impl;
    self->ir_transmit_ack = ir_transmit_ack_impl;
```

Add the implementations (after `action_ack_impl`'s body, before `telemetry_impl`):

```c
static dom_models_error_t ir_capture_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const int32_t*                     raw_data,
    size_t                              raw_data_count
) {
    if (!self || !self->ctx || !device_id || device_id[0] == '\0' || !raw_data || raw_data_count == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = self->ctx;

    char               topic[INF_MESSAGING_DEF_PUB_MQTT_IMPL_TOPIC_MAX_LEN];
    dom_models_error_t err = inf_messaging_def_pub_mqtt_impl_build_device_topic(device_id, "ir/rx", topic, sizeof(topic));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return inf_messaging_def_pub_mqtt_impl_publish_json(
        ctx,
        topic,
        inf_messaging_def_pub_mqtt_impl_build_ir_capture_json(raw_data, raw_data_count),
        INF_MESSAGING_DEF_PUB_MQTT_IMPL_QOS_DEFAULT,
        false
    );
}

static dom_models_error_t ir_transmit_ack_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        execution_id,
    const char*                        status,
    const char*                        message
) {
    if (!self || !self->ctx || !device_id || device_id[0] == '\0' || !execution_id || execution_id[0] == '\0' || !status || status[0] == '\0') {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = self->ctx;

    char               topic[INF_MESSAGING_DEF_PUB_MQTT_IMPL_TOPIC_MAX_LEN];
    dom_models_error_t err = inf_messaging_def_pub_mqtt_impl_build_device_topic(device_id, "ir/tx_ack", topic, sizeof(topic));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return inf_messaging_def_pub_mqtt_impl_publish_json(
        ctx,
        topic,
        inf_messaging_def_pub_mqtt_impl_build_ir_transmit_ack_json(execution_id, status, message),
        INF_MESSAGING_DEF_PUB_MQTT_IMPL_QOS_DEFAULT,
        false
    );
}
```

- [ ] **Step 5: Wire the new `def_sub` contract method in `mqtt_impl.c`**

In `main/src/infrastructure/messaging/def_sub/mqtt_impl.c`, add a static prototype (after `config_impl`'s):

```c
static dom_models_error_t ir_tx_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
);
```

Assign it in `inf_messaging_def_sub_mqtt_impl_new` (after `self->config = config_impl;`):

```c
    self->ir_tx = ir_tx_impl;
```

Add the implementation (after `config_impl`'s body):

```c
static dom_models_error_t ir_tx_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_sub_mqtt_impl_subscribe_suffix(self->ctx, device_id, "ir/tx");
}
```

- [ ] **Step 6: Extend the `def_pub` stub impl**

In `main/include/infrastructure/messaging/def_pub/stub_impl_types.h`, add `#include <stdint.h>`, a new max-len constant, and new fields to `inf_messaging_def_pub_stub_impl_ctx_t`:

```c
#define INF_MESSAGING_DEF_PUB_STUB_IMPL_IR_RAW_DATA_MAX_LEN 64
```
```c
    char    last_ir_capture_device_id[INF_MESSAGING_DEF_PUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    int32_t last_ir_capture_raw_data[INF_MESSAGING_DEF_PUB_STUB_IMPL_IR_RAW_DATA_MAX_LEN];
    size_t  last_ir_capture_raw_data_len;
    size_t  ir_capture_publish_cnt;
    char    last_ir_transmit_ack_device_id[INF_MESSAGING_DEF_PUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    char    last_ir_transmit_ack_execution_id[INF_MESSAGING_DEF_PUB_STUB_IMPL_STR_MAX_LEN];
    char    last_ir_transmit_ack_status[INF_MESSAGING_DEF_PUB_STUB_IMPL_STR_MAX_LEN];
    char    last_ir_transmit_ack_message[INF_MESSAGING_DEF_PUB_STUB_IMPL_STR_MAX_LEN];
    size_t  ir_transmit_ack_publish_cnt;
```
(add these right after the existing `last_action_ack_message` / before `last_telemetry_device_id` fields — order within the struct doesn't matter functionally).

In `main/include/infrastructure/messaging/def_pub/stub_impl_utils.h`, add `#include <stdint.h>` and two declarations:

```c
dom_models_error_t inf_messaging_def_pub_stub_impl_set_ir_capture(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const int32_t*                         raw_data,
    size_t                                  raw_data_count
);

dom_models_error_t inf_messaging_def_pub_stub_impl_set_ir_transmit_ack(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const char*                            execution_id,
    const char*                            status,
    const char*                            message
);
```

In `main/src/infrastructure/messaging/def_pub/stub_impl_utils.c`, add the implementations (after `set_action_ack`'s body):

```c
dom_models_error_t inf_messaging_def_pub_stub_impl_set_ir_capture(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const int32_t*                         raw_data,
    size_t                                  raw_data_count
) {
    if (!ctx || !cstr_available(device_id) || !raw_data || raw_data_count == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    copy_cstr(ctx->last_ir_capture_device_id, sizeof(ctx->last_ir_capture_device_id), device_id);

    size_t copy_count = raw_data_count < INF_MESSAGING_DEF_PUB_STUB_IMPL_IR_RAW_DATA_MAX_LEN
                             ? raw_data_count
                             : INF_MESSAGING_DEF_PUB_STUB_IMPL_IR_RAW_DATA_MAX_LEN;
    memcpy(ctx->last_ir_capture_raw_data, raw_data, copy_count * sizeof(int32_t));
    ctx->last_ir_capture_raw_data_len = copy_count;
    ctx->ir_capture_publish_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_pub_stub_impl_set_ir_transmit_ack(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const char*                            execution_id,
    const char*                            status,
    const char*                            message
) {
    if (!ctx || !cstr_available(device_id) || !cstr_available(execution_id) || !cstr_available(status)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    copy_cstr(ctx->last_ir_transmit_ack_device_id, sizeof(ctx->last_ir_transmit_ack_device_id), device_id);
    copy_cstr(ctx->last_ir_transmit_ack_execution_id, sizeof(ctx->last_ir_transmit_ack_execution_id), execution_id);
    copy_cstr(ctx->last_ir_transmit_ack_status, sizeof(ctx->last_ir_transmit_ack_status), status);
    copy_cstr(ctx->last_ir_transmit_ack_message, sizeof(ctx->last_ir_transmit_ack_message), message);
    ctx->ir_transmit_ack_publish_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}
```

In `main/src/infrastructure/messaging/def_pub/stub_impl.c`, add prototypes, assignments (`self->ir_capture = ir_capture_impl; self->ir_transmit_ack = ir_transmit_ack_impl;` after the existing `self->action_ack = action_ack_impl;`), and thin wrapper implementations following the exact shape of `action_ack_impl` in that same file:

```c
static dom_models_error_t ir_capture_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const int32_t*                     raw_data,
    size_t                              raw_data_count
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_pub_stub_impl_set_ir_capture(self->ctx, device_id, raw_data, raw_data_count);
}

static dom_models_error_t ir_transmit_ack_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        execution_id,
    const char*                        status,
    const char*                        message
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_pub_stub_impl_set_ir_transmit_ack(self->ctx, device_id, execution_id, status, message);
}
```
(add `#include <stdint.h>` to this file's includes if not already pulled in transitively.)

- [ ] **Step 7: Extend the `def_sub` stub impl**

In `main/include/infrastructure/messaging/def_sub/stub_impl_types.h`, add to the cfg struct, its `CFG_DEFAULT()` macro, and the ctx struct:

```c
    bool ir_tx_subscribed;   /* add to both cfg and ctx structs */
```
```c
        .ir_tx_subscribed            = false,   /* add to INF_MESSAGING_DEF_SUB_STUB_IMPL_CFG_DEFAULT() */
```
```c
    char   last_ir_tx_device_id[INF_MESSAGING_DEF_SUB_STUB_IMPL_DEVICE_ID_MAX_LEN];  /* add to ctx struct */
    size_t ir_tx_subscribe_cnt;                                                       /* add to ctx struct */
```

In `main/include/infrastructure/messaging/def_sub/stub_impl_utils.h`, add:

```c
dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_ir_tx(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
);
```

In `main/src/infrastructure/messaging/def_sub/stub_impl_utils.c`, add (after `subscribe_config`'s body), and also copy `ctx->ir_tx_subscribed = cfg->ir_tx_subscribed;` into `inf_messaging_def_sub_stub_impl_load_cfg`:

```c
dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_ir_tx(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
) {
    if (!ctx || !cstr_available(device_id)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    ctx->ir_tx_subscribed = true;
    copy_cstr(ctx->last_ir_tx_device_id, sizeof(ctx->last_ir_tx_device_id), device_id);
    ctx->ir_tx_subscribe_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}
```

In `main/src/infrastructure/messaging/def_sub/stub_impl.c`, add the prototype, `self->ir_tx = ir_tx_impl;` assignment, and:

```c
static dom_models_error_t ir_tx_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_sub_stub_impl_subscribe_ir_tx(self->ctx, device_id);
}
```

- [ ] **Step 8: Build**

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure && idf.py build
```
Expected: succeeds with no new warnings.

- [ ] **Step 9: Format and commit**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
clang-format -i \
  main/include/domain/contracts/messaging/def_pub.h \
  main/include/domain/contracts/messaging/def_sub.h \
  main/include/infrastructure/messaging/def_pub/mqtt_impl_utils.h \
  main/src/infrastructure/messaging/def_pub/mqtt_impl_utils.c \
  main/src/infrastructure/messaging/def_pub/mqtt_impl.c \
  main/src/infrastructure/messaging/def_sub/mqtt_impl.c \
  main/include/infrastructure/messaging/def_pub/stub_impl_types.h \
  main/include/infrastructure/messaging/def_pub/stub_impl_utils.h \
  main/src/infrastructure/messaging/def_pub/stub_impl_utils.c \
  main/src/infrastructure/messaging/def_pub/stub_impl.c \
  main/include/infrastructure/messaging/def_sub/stub_impl_types.h \
  main/include/infrastructure/messaging/def_sub/stub_impl_utils.h \
  main/src/infrastructure/messaging/def_sub/stub_impl_utils.c \
  main/src/infrastructure/messaging/def_sub/stub_impl.c
git diff --check
git add main/include/domain/contracts/messaging main/include/infrastructure/messaging main/src/infrastructure/messaging
git commit -m "$(cat <<'EOF'
feat: (ir) extend def_pub/def_sub contracts with ir/rx, ir/tx, ir/tx_ack

Adds ir_capture and ir_transmit_ack to def_pub, ir_tx to def_sub, with
MQTT and stub implementations for both. No consumer yet - wired up in
later commits.
EOF
)"
```

---

## Task 3: `domain/models/ir.h` + `domain/contracts/device/ir.h` + `infrastructure/device/ir/esp_impl`

**Files:**
- Create: `main/include/domain/models/ir.h`
- Create: `main/include/domain/contracts/device/ir.h`
- Create: `main/include/infrastructure/device/ir/esp_impl_types.h`
- Create: `main/include/infrastructure/device/ir/esp_impl.h`
- Create: `main/src/infrastructure/device/ir/esp_impl.c`
- Modify: `main/CMakeLists.txt`

**Interfaces:**
- Consumes (from Task 1): `infrared_handle_t`, `infrared_cfg_t`, `INFRARED_CFG_DEFAULT()`, `infrared_new/_init/_deinit/_delete/_transmit`, `infrared_duration_t`, `infrared_rx_frame_t`.
- Produces (consumed by Task 4): `dom_models_ir_duration_t{bool level; uint32_t duration_us;}`, `dom_models_ir_receive_cb_t`, `dom_contracts_device_ir_t.transmit(self, durations, count)`, `dom_contracts_device_ir_t.set_receive_handler(self, cb_ctx, cb)`, `inf_device_ir_esp_impl_new(NULL)` / `_init` / `_deinit` / `_delete`.

- [ ] **Step 1: Write `domain/models/ir.h`**

```c
#ifndef DOMAIN_MODELS_IR_H
#define DOMAIN_MODELS_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool     level;
    uint32_t duration_us;
} dom_models_ir_duration_t;

typedef void (*dom_models_ir_receive_cb_t)(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    void*                           cb_ctx
);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_IR_H */
```

- [ ] **Step 2: Write `domain/contracts/device/ir.h`**

```c
#ifndef DOMAIN_CONTRACTS_DEVICE_IR_H
#define DOMAIN_CONTRACTS_DEVICE_IR_H

#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_device_ir_t dom_contracts_device_ir_t;

struct dom_contracts_device_ir_t {
    void* ctx;
    dom_models_error_t (*transmit)(
        dom_contracts_device_ir_t*      self,
        const dom_models_ir_duration_t* durations,
        size_t                          duration_count
    );
    dom_models_error_t (*set_receive_handler)(
        dom_contracts_device_ir_t* self,
        void*                      cb_ctx,
        dom_models_ir_receive_cb_t cb
    );
};

static inline dom_contracts_device_ir_t* dom_contracts_device_ir_new(void* ctx) {
    dom_contracts_device_ir_t* self = (dom_contracts_device_ir_t*)calloc(1, sizeof(dom_contracts_device_ir_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_device_ir_delete(dom_contracts_device_ir_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_DEVICE_IR_H */
```

- [ ] **Step 3: Write `infrastructure/device/ir/esp_impl_types.h`**

RX/TX GPIOs are hardcoded here per the design decision (no Kconfig). GPIO4/GPIO5 are free, non-strapping, non-flash pins on the ESP32-C3 reference target this repo already builds for — adjust these two `#define`s if a different PCB revision wires IR to different pins.

```c
#ifndef INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_TYPES_H
#define INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_TYPES_H

#include "domain/models/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INF_DEVICE_IR_ESP_IMPL_RX_GPIO GPIO_NUM_4
#define INF_DEVICE_IR_ESP_IMPL_TX_GPIO GPIO_NUM_5

typedef struct {
    dom_models_ir_receive_cb_t receive_cb;
    void*                      receive_cb_ctx;
    void*                      infrared;   /* infrared_handle_t*, opaque here so this
                                               header doesn't leak the component's
                                               ESP-IDF-native type into domain-adjacent
                                               includes */
    bool                       initialized;
} inf_device_ir_esp_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_TYPES_H */
```

- [ ] **Step 4: Write `infrastructure/device/ir/esp_impl.h`**

```c
#ifndef INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_H
#define INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_H

#include "domain/contracts/device/ir.h"
#include "infrastructure/device/ir/esp_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_device_ir_t* inf_device_ir_esp_impl_new(void* unused_cfg);

void inf_device_ir_esp_impl_delete(dom_contracts_device_ir_t* self);

dom_models_error_t inf_device_ir_esp_impl_init(dom_contracts_device_ir_t* self);

void inf_device_ir_esp_impl_deinit(dom_contracts_device_ir_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_H */
```
(`unused_cfg` mirrors `inf_system_info_esp_impl_new(NULL)` / `inf_device_wifi_esp_impl_new(NULL)`'s existing pattern in this repo for infra impls with no meaningful runtime config — kept for call-site consistency with `composition/*` even though this impl has nothing to configure per-instance.)

- [ ] **Step 5: Write `infrastructure/device/ir/esp_impl.c`**

```c
#include "infrastructure/device/ir/esp_impl.h"

#include <stdlib.h>

#include "domain/models/error.h"
#include "infrared.h"

#define BASE_TAG "inf_device_ir_esp_impl"

/* Contract Function Prototypes */

static dom_models_error_t transmit_impl(
    dom_contracts_device_ir_t*      self,
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count
);
static dom_models_error_t set_receive_handler_impl(
    dom_contracts_device_ir_t* self,
    void*                      cb_ctx,
    dom_models_ir_receive_cb_t cb
);

/* Helper Function Prototypes */

static dom_models_error_t map_error(esp_err_t err);

static void on_receive(const infrared_rx_frame_t* frame, void* user_ctx);

/* Constructor and Destructor */

dom_contracts_device_ir_t* inf_device_ir_esp_impl_new(void* unused_cfg) {
    (void)unused_cfg;

    inf_device_ir_esp_impl_ctx_t* ctx = (inf_device_ir_esp_impl_ctx_t*)calloc(1, sizeof(inf_device_ir_esp_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    dom_contracts_device_ir_t* self = dom_contracts_device_ir_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->transmit             = transmit_impl;
    self->set_receive_handler  = set_receive_handler_impl;

    return self;
}

void inf_device_ir_esp_impl_delete(dom_contracts_device_ir_t* self) {
    if (!self) {
        return;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        if (ctx->infrared) {
            infrared_delete((infrared_handle_t*)ctx->infrared);
        }
        free(ctx);
    }

    dom_contracts_device_ir_delete(self);
}

/* Lifecycle */

dom_models_error_t inf_device_ir_esp_impl_init(dom_contracts_device_ir_t* self) {
    const char* tag = BASE_TAG "/init";

    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    if (ctx->initialized) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    infrared_cfg_t cfg = INFRARED_CFG_DEFAULT();
    cfg.rx_gpio    = INF_DEVICE_IR_ESP_IMPL_RX_GPIO;
    cfg.tx_gpio    = INF_DEVICE_IR_ESP_IMPL_TX_GPIO;
    cfg.enable_rx  = true;
    cfg.enable_tx  = true;
    cfg.on_receive = on_receive;
    cfg.user_ctx   = ctx;

    infrared_handle_t* infrared = infrared_new(&cfg);
    if (!infrared) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    esp_err_t err = infrared_init(infrared);
    if (err != ESP_OK) {
        infrared_delete(infrared);
        return map_error(err);
    }

    ctx->infrared    = infrared;
    ctx->initialized = true;

    return DOMAIN_MODELS_ERROR_OK;
}

void inf_device_ir_esp_impl_deinit(dom_contracts_device_ir_t* self) {
    if (!self || !self->ctx) {
        return;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    if (!ctx->initialized) {
        return;
    }

    if (ctx->infrared) {
        infrared_deinit((infrared_handle_t*)ctx->infrared);
    }
    ctx->initialized = false;
}

/* Contract Function Implementations */

static dom_models_error_t transmit_impl(
    dom_contracts_device_ir_t*      self,
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count
) {
    if (!self || !self->ctx || !durations || duration_count == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    if (!ctx->initialized || !ctx->infrared) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    infrared_duration_t* converted = (infrared_duration_t*)calloc(duration_count, sizeof(infrared_duration_t));
    if (!converted) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    for (size_t i = 0; i < duration_count; i++) {
        converted[i].level       = durations[i].level;
        converted[i].duration_us = durations[i].duration_us;
    }

    esp_err_t err = infrared_transmit((infrared_handle_t*)ctx->infrared, converted, duration_count);
    free(converted);

    return map_error(err);
}

static dom_models_error_t set_receive_handler_impl(
    dom_contracts_device_ir_t* self,
    void*                      cb_ctx,
    dom_models_ir_receive_cb_t cb
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    ctx->receive_cb                   = cb;
    ctx->receive_cb_ctx               = cb_ctx;

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static dom_models_error_t map_error(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return DOMAIN_MODELS_ERROR_OK;
        case ESP_ERR_INVALID_ARG:
            return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        case ESP_ERR_INVALID_STATE:
            return DOMAIN_MODELS_ERROR_BAD_STATE;
        case ESP_ERR_NO_MEM:
            return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
        case ESP_ERR_TIMEOUT:
            return DOMAIN_MODELS_ERROR_TIMEOUT;
        default:
            return DOMAIN_MODELS_ERROR_FAILURE;
    }
}

static void on_receive(const infrared_rx_frame_t* frame, void* user_ctx) {
    inf_device_ir_esp_impl_ctx_t* ctx = (inf_device_ir_esp_impl_ctx_t*)user_ctx;
    if (!ctx || !frame || !ctx->receive_cb) {
        return;
    }
    if (frame->overflowed || !frame->durations || frame->duration_count == 0) {
        return;
    }

    dom_models_ir_duration_t* converted = (dom_models_ir_duration_t*)calloc(frame->duration_count, sizeof(dom_models_ir_duration_t));
    if (!converted) {
        return;
    }

    for (size_t i = 0; i < frame->duration_count; i++) {
        converted[i].level       = frame->durations[i].level;
        converted[i].duration_us = frame->durations[i].duration_us;
    }

    ctx->receive_cb(converted, frame->duration_count, ctx->receive_cb_ctx);
    free(converted);
}
```

- [ ] **Step 6: Pull the `infrared` component into the `main` component's build**

In `main/CMakeLists.txt`, under the `PRIV_REQUIRES` list's existing (empty) `# components` comment line, add:

```cmake
        # components
        infrared
```

- [ ] **Step 7: Build**

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure && idf.py build
```
Expected: succeeds. This is the first real compile of `components/infrared` (Task 1) — any typo carried over from the copy would surface here.

- [ ] **Step 8: Format and commit**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
clang-format -i \
  main/include/domain/models/ir.h \
  main/include/domain/contracts/device/ir.h \
  main/include/infrastructure/device/ir/esp_impl_types.h \
  main/include/infrastructure/device/ir/esp_impl.h \
  main/src/infrastructure/device/ir/esp_impl.c
git diff --check
git add main/include/domain/models/ir.h main/include/domain/contracts/device/ir.h main/include/infrastructure/device/ir main/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: (ir) add device/ir contract and ESP-IDF infrastructure impl

Wraps the ported infrared component behind dom_contracts_device_ir_t.
Pulls components/infrared into the main component's build. Not yet
wired into any composition.
EOF
)"
```

---

## Task 4: `domain/usecases/internal/infrared.h` + `application/internal/infrared/impl`

**Files:**
- Create: `main/include/domain/usecases/internal/infrared.h`
- Create: `main/include/application/internal/infrared/impl_types.h`
- Create: `main/include/application/internal/infrared/impl_utils.h`
- Create: `main/src/application/internal/infrared/impl_utils.c`
- Create: `main/include/application/internal/infrared/impl.h`
- Create: `main/src/application/internal/infrared/impl.c`

**Interfaces:**
- Consumes (from Task 2): `dom_contracts_messaging_def_pub_t.ir_capture/.ir_transmit_ack`, `dom_contracts_messaging_def_sub_t.ir_tx`. Consumes (from Task 3): `dom_contracts_device_ir_t.transmit/.set_receive_handler`, `dom_models_ir_duration_t`.
- Produces (consumed by Task 6): `dom_usecases_internal_infrared_t.subscribe(self)`, `dom_usecases_internal_infrared_t.transmit(self, execution_id, raw_data, raw_data_count)`, `app_internal_infrared_impl_new(cfg)` / `_init` / `_deinit` / `_delete`, `app_internal_infrared_impl_cfg_t{logger, ir, def_pub, def_sub, preloaded_repository}`.

- [ ] **Step 1: Write `domain/usecases/internal/infrared.h`**

```c
#ifndef DOMAIN_USECASES_INTERNAL_INFRARED_H
#define DOMAIN_USECASES_INTERNAL_INFRARED_H

#include <stdint.h>
#include <stdlib.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_usecases_internal_infrared_t dom_usecases_internal_infrared_t;

struct dom_usecases_internal_infrared_t {
    void* ctx;
    dom_models_error_t (*subscribe)(
        dom_usecases_internal_infrared_t* self
    );
    dom_models_error_t (*transmit)(
        dom_usecases_internal_infrared_t* self,
        const char*                       execution_id,
        const int32_t*                    raw_data,
        size_t                            raw_data_count
    );
};

static inline dom_usecases_internal_infrared_t* dom_usecases_internal_infrared_new(void* ctx) {
    dom_usecases_internal_infrared_t* self = (dom_usecases_internal_infrared_t*)calloc(1, sizeof(dom_usecases_internal_infrared_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_infrared_delete(dom_usecases_internal_infrared_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_INFRARED_H */
```

- [ ] **Step 2: Write `application/internal/infrared/impl_types.h`**

```c
#ifndef APPLICATION_INTERNAL_INFRARED_IMPL_TYPES_H
#define APPLICATION_INTERNAL_INFRARED_IMPL_TYPES_H

#include "domain/contracts/device/ir.h"
#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/messaging/def_pub.h"
#include "domain/contracts/messaging/def_sub.h"
#include "domain/contracts/repository/preloaded.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dom_contracts_logger_leveled_t*       logger;
    dom_contracts_device_ir_t*            ir;
    dom_contracts_messaging_def_pub_t*    def_pub;
    dom_contracts_messaging_def_sub_t*    def_sub;
    dom_contracts_repository_preloaded_t* preloaded_repository;
} app_internal_infrared_impl_cfg_t;

typedef struct {
    app_internal_infrared_impl_cfg_t cfg;
    char                              device_id_str[37];
    bool                              receive_handler_registered;
} app_internal_infrared_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_INFRARED_IMPL_TYPES_H */
```

- [ ] **Step 3: Write `application/internal/infrared/impl_utils.h`**

```c
#ifndef APPLICATION_INTERNAL_INFRARED_IMPL_UTILS_H
#define APPLICATION_INTERNAL_INFRARED_IMPL_UTILS_H

#include <stddef.h>
#include <stdint.h>

#include "application/internal/infrared/impl_types.h"
#include "domain/models/error.h"
#include "domain/models/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_infrared_impl_validate_cfg(const app_internal_infrared_impl_cfg_t* cfg);

/* Rebuilds (level, duration) pairs from a flat alternating raw_data array,
   starting with a mark (level=true) at index 0. out must have capacity for
   raw_data_count entries. */
dom_models_error_t app_internal_infrared_impl_raw_data_to_durations(
    const int32_t*             raw_data,
    size_t                     raw_data_count,
    dom_models_ir_duration_t* out
);

/* Strips the level field, producing the flat alternating array the backend
   expects. out must have capacity for duration_count entries. */
dom_models_error_t app_internal_infrared_impl_durations_to_raw_data(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    int32_t*                        out
);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_INFRARED_IMPL_UTILS_H */
```

- [ ] **Step 4: Write `application/internal/infrared/impl_utils.c`**

```c
#include "application/internal/infrared/impl_utils.h"

dom_models_error_t app_internal_infrared_impl_validate_cfg(const app_internal_infrared_impl_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->ir || !cfg->def_pub || !cfg->def_sub || !cfg->preloaded_repository) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_infrared_impl_raw_data_to_durations(
    const int32_t*             raw_data,
    size_t                     raw_data_count,
    dom_models_ir_duration_t* out
) {
    if (!raw_data || raw_data_count == 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    for (size_t i = 0; i < raw_data_count; i++) {
        if (raw_data[i] < 0) {
            return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        }
        out[i].level       = (i % 2) == 0; /* index 0 is always a mark */
        out[i].duration_us = (uint32_t)raw_data[i];
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_infrared_impl_durations_to_raw_data(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    int32_t*                        out
) {
    if (!durations || duration_count == 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    for (size_t i = 0; i < duration_count; i++) {
        out[i] = (int32_t)durations[i].duration_us;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 5: Write `application/internal/infrared/impl.h`**

```c
#ifndef APPLICATION_INTERNAL_INFRARED_IMPL_H
#define APPLICATION_INTERNAL_INFRARED_IMPL_H

#include "application/internal/infrared/impl_types.h"
#include "domain/usecases/internal/infrared.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_infrared_t* app_internal_infrared_impl_new(const app_internal_infrared_impl_cfg_t* cfg);

void app_internal_infrared_impl_delete(dom_usecases_internal_infrared_t* self);

dom_models_error_t app_internal_infrared_impl_init(dom_usecases_internal_infrared_t* self);

void app_internal_infrared_impl_deinit(dom_usecases_internal_infrared_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_INFRARED_IMPL_H */
```

- [ ] **Step 6: Write `application/internal/infrared/impl.c`**

```c
#include "application/internal/infrared/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/infrared/impl_utils.h"
#include "domain/models/error.h"
#include "domain/models/ir.h"

#define BASE_TAG                  "app_internal_infrared"
#define IR_TRANSMIT_ACK_SUCCESS   "SUCCESS"
#define IR_TRANSMIT_ACK_FAILED    "FAILED"
#define IR_RAW_DATA_MAX_LEN       512 /* matches INFRARED_RX_MAX_DURATIONS */

/* Contract Function Prototypes */

static dom_models_error_t subscribe_impl(
    dom_usecases_internal_infrared_t* self
);
static dom_models_error_t transmit_impl(
    dom_usecases_internal_infrared_t* self,
    const char*                       execution_id,
    const int32_t*                    raw_data,
    size_t                            raw_data_count
);

/* Helper Function Prototypes */

static void on_ir_receive(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    void*                           cb_ctx
);

/* Constructor and Destructor */

dom_usecases_internal_infrared_t* app_internal_infrared_impl_new(const app_internal_infrared_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    if (app_internal_infrared_impl_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_infrared_impl_ctx_t* ctx = (app_internal_infrared_impl_ctx_t*)calloc(1, sizeof(app_internal_infrared_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_infrared_impl_cfg_t));

    dom_models_error_t err = ctx->cfg.preloaded_repository->get_device_id_str(ctx->cfg.preloaded_repository, ctx->device_id_str, sizeof(ctx->device_id_str));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load device id: %s (%d)", dom_models_error_str(err), (int)err);
        free(ctx);
        return NULL;
    }

    dom_usecases_internal_infrared_t* self = dom_usecases_internal_infrared_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->subscribe = subscribe_impl;
    self->transmit  = transmit_impl;

    return self;
}

void app_internal_infrared_impl_delete(dom_usecases_internal_infrared_t* self) {
    if (!self) {
        return;
    }

    app_internal_infrared_impl_deinit(self);

    free(self->ctx);
    dom_usecases_internal_infrared_delete(self);
}

/* Lifecycle */

dom_models_error_t app_internal_infrared_impl_init(dom_usecases_internal_infrared_t* self) {
    const char* tag = BASE_TAG "/init";

    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    app_internal_infrared_impl_ctx_t* ctx = self->ctx;
    if (ctx->receive_handler_registered) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    dom_models_error_t err = ctx->cfg.ir->set_receive_handler(ctx->cfg.ir, ctx, on_ir_receive);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to register IR receive handler: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->receive_handler_registered = true;

    return DOMAIN_MODELS_ERROR_OK;
}

void app_internal_infrared_impl_deinit(dom_usecases_internal_infrared_t* self) {
    if (!self || !self->ctx) {
        return;
    }

    app_internal_infrared_impl_ctx_t* ctx = self->ctx;
    if (!ctx->receive_handler_registered) {
        return;
    }

    (void)ctx->cfg.ir->set_receive_handler(ctx->cfg.ir, NULL, NULL);
    ctx->receive_handler_registered = false;
}

/* Contract Function Implementations */

static dom_models_error_t subscribe_impl(
    dom_usecases_internal_infrared_t* self
) {
    const char* tag = BASE_TAG "/subscribe";

    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    app_internal_infrared_impl_ctx_t* ctx = self->ctx;

    dom_models_error_t err = ctx->cfg.def_sub->ir_tx(ctx->device_id_str, ctx->cfg.def_sub);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to ir/tx: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Subscribed to ir/tx successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t transmit_impl(
    dom_usecases_internal_infrared_t* self,
    const char*                       execution_id,
    const int32_t*                    raw_data,
    size_t                            raw_data_count
) {
    const char* tag = BASE_TAG "/transmit";

    if (!self || !self->ctx || !execution_id || execution_id[0] == '\0' || !raw_data || raw_data_count == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (raw_data_count > IR_RAW_DATA_MAX_LEN) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    app_internal_infrared_impl_ctx_t* ctx = self->ctx;

    dom_models_ir_duration_t durations[IR_RAW_DATA_MAX_LEN];
    dom_models_error_t       err = app_internal_infrared_impl_raw_data_to_durations(raw_data, raw_data_count, durations);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Invalid raw_data payload: %s (%d)", dom_models_error_str(err), (int)err);
        (void)ctx->cfg.def_pub->ir_transmit_ack(ctx->cfg.def_pub, ctx->device_id_str, execution_id, IR_TRANSMIT_ACK_FAILED, "invalid raw_data payload");
        return err;
    }

    err = ctx->cfg.ir->transmit(ctx->cfg.ir, durations, raw_data_count);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "IR transmit failed: %s (%d)", dom_models_error_str(err), (int)err);
        (void)ctx->cfg.def_pub->ir_transmit_ack(ctx->cfg.def_pub, ctx->device_id_str, execution_id, IR_TRANSMIT_ACK_FAILED, dom_models_error_str(err));
        return err;
    }

    err = ctx->cfg.def_pub->ir_transmit_ack(ctx->cfg.def_pub, ctx->device_id_str, execution_id, IR_TRANSMIT_ACK_SUCCESS, NULL);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to publish ir/tx_ack: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "IR transmit completed successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static void on_ir_receive(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    void*                           cb_ctx
) {
    app_internal_infrared_impl_ctx_t* ctx = (app_internal_infrared_impl_ctx_t*)cb_ctx;
    if (!ctx || !durations || duration_count == 0 || duration_count > IR_RAW_DATA_MAX_LEN) {
        return;
    }

    int32_t raw_data[IR_RAW_DATA_MAX_LEN];
    if (app_internal_infrared_impl_durations_to_raw_data(durations, duration_count, raw_data) != DOMAIN_MODELS_ERROR_OK) {
        return;
    }

    /* Best-effort: this runs from the RX worker task, not a caller waiting
       on a return value - matches messaging_callbacks_impl.c's
       on_log_message, which also fires-and-forgets its def_pub call. */
    (void)ctx->cfg.def_pub->ir_capture(ctx->cfg.def_pub, ctx->device_id_str, raw_data, duration_count);
}
```

- [ ] **Step 7: Build**

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure && idf.py build
```
Expected: succeeds.

- [ ] **Step 8: Format and commit**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
clang-format -i \
  main/include/domain/usecases/internal/infrared.h \
  main/include/application/internal/infrared/impl_types.h \
  main/include/application/internal/infrared/impl_utils.h \
  main/src/application/internal/infrared/impl_utils.c \
  main/include/application/internal/infrared/impl.h \
  main/src/application/internal/infrared/impl.c
git diff --check
git add main/include/domain/usecases/internal/infrared.h main/include/application/internal/infrared main/src/application/internal/infrared
git commit -m "$(cat <<'EOF'
feat: (ir) add infrared usecase: subscribe to ir/tx, publish ir/rx and
ir/tx_ack

Rebuilds/flattens the mark-space duration array at the raw_data <->
dom_models_ir_duration_t boundary. Not yet wired into any composition.
EOF
)"
```

---

## Task 5: Presentation — `ir/tx` MQTT handler + context wiring

**Files:**
- Modify: `main/include/presentation/mqtt/context.h`
- Modify: `main/src/presentation/mqtt/context.c`
- Modify: `main/include/presentation/mqtt/context_utils.h`
- Modify: `main/src/presentation/mqtt/context_utils.c`
- Modify: `main/src/presentation/mqtt/event/on_message.c`
- Create: `main/include/presentation/mqtt/handler/ir_tx/dto.h`
- Create: `main/src/presentation/mqtt/handler/ir_tx/dto.c`
- Create: `main/include/presentation/mqtt/handler/ir_tx/handler.h`
- Create: `main/src/presentation/mqtt/handler/ir_tx/handler.c`
- Modify: `main/src/composition/main/presentation.c`
- Modify: `main/src/composition/counter_test/presentation.c`

**Interfaces:**
- Consumes (from Task 2, 4): `dom_contracts_messaging_def_pub_t.ir_transmit_ack`, `dom_usecases_internal_infrared_t.transmit`.
- Produces (consumed by Task 6): `pres_mqtt_context_new(logger, preloaded_repository, messaging_callbacks, settings, ota, def_pub, infrared)` (note the two new trailing params — `infrared` may be `NULL`), `pres_mqtt_handler_ir_tx(ctx, data, data_len)`.

- [ ] **Step 1: Extend `pres_mqtt_context_t` and its constructor**

In `main/include/presentation/mqtt/context.h`, add includes `"domain/contracts/messaging/def_pub.h"` and `"domain/usecases/internal/infrared.h"`. Add fields to the struct (after `settings`, before `mqtt_client`):

```c
    dom_contracts_messaging_def_pub_t*    def_pub;
    dom_usecases_internal_infrared_t*     infrared; /* nullable: NULL on
                                                         compositions that
                                                         don't wire IR */
```

Add a topic buffer field (alongside `config_topic`):

```c
    char                                         ir_tx_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
```

Update the constructor declaration:

```c
pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota,
    dom_contracts_messaging_def_pub_t*           def_pub,
    dom_usecases_internal_infrared_t*            infrared
);
```

- [ ] **Step 2: Update `pres_mqtt_context_validate_cfg`**

In `main/include/presentation/mqtt/context_utils.h`, add `#include "domain/contracts/messaging/def_pub.h"` and add `dom_contracts_messaging_def_pub_t* def_pub` as a parameter (do **not** add `infrared` — it's intentionally unvalidated/nullable):

```c
dom_models_error_t pres_mqtt_context_validate_cfg(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota,
    dom_contracts_messaging_def_pub_t*           def_pub
);
```

In `main/src/presentation/mqtt/context_utils.c`, add the parameter and the null check:

```c
dom_models_error_t pres_mqtt_context_validate_cfg(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota,
    dom_contracts_messaging_def_pub_t*           def_pub
) {
    if (!logger || !preloaded_repository || !messaging_callbacks || !settings || !ota || !def_pub) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 3: Update `pres_mqtt_context_new`/`_init` in `context.c`**

In `main/src/presentation/mqtt/context.c`, update the function signature to match Step 1, pass `def_pub` through to `pres_mqtt_context_validate_cfg`, and store the two new fields:

```c
pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota,
    dom_contracts_messaging_def_pub_t*           def_pub,
    dom_usecases_internal_infrared_t*            infrared
) {
    if (pres_mqtt_context_validate_cfg(logger, preloaded_repository, messaging_callbacks, settings, ota, def_pub) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    pres_mqtt_context_t* self = (pres_mqtt_context_t*)calloc(1, sizeof(pres_mqtt_context_t));
    if (!self) {
        return NULL;
    }

    self->logger               = logger;
    self->preloaded_repository = preloaded_repository;
    self->messaging_callbacks  = messaging_callbacks;
    self->settings             = settings;
    self->ota                  = ota;
    self->def_pub               = def_pub;
    self->infrared              = infrared;
    self->mqtt_client          = NULL;

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
```

In the same file's `pres_mqtt_context_init`, add the `ir_tx_topic` build (after the `config_topic` block, before the `esp_mqtt_client_register_event` call):

```c
    written = snprintf(self->ir_tx_topic, sizeof(self->ir_tx_topic), "/sub/%s/ir/tx", self->device_id_str);
    if (written <= 0 || (size_t)written >= sizeof(self->ir_tx_topic)) {
        self->logger->error(self->logger, tag, "Failed to build ir_tx topic string");
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
```

- [ ] **Step 4: Write the `ir_tx` DTO**

`main/include/presentation/mqtt/handler/ir_tx/dto.h`:

```c
#ifndef PRESENTATION_MQTT_HANDLER_IR_TX_DTO_H
#define PRESENTATION_MQTT_HANDLER_IR_TX_DTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_MQTT_HANDLER_IR_TX_DTO_EXECUTION_ID_MAX_LEN 64
#define PRES_MQTT_HANDLER_IR_TX_DTO_RAW_DATA_MAX_LEN      512

typedef struct {
    char    execution_id[PRES_MQTT_HANDLER_IR_TX_DTO_EXECUTION_ID_MAX_LEN];
    bool    execution_id_set;
    int32_t raw_data[PRES_MQTT_HANDLER_IR_TX_DTO_RAW_DATA_MAX_LEN];
    size_t  raw_data_count;
} pres_mqtt_handler_ir_tx_dto_request_t;

/* Parses the /sub/<device_id>/ir/tx payload:
   {"execution_id": string, "raw_data": [int, ...]}. Returns
   DOMAIN_MODELS_ERROR_BAD_ARGUMENT if data is empty/not valid JSON; a
   missing execution_id or raw_data is reported via execution_id_set /
   raw_data_count == 0 instead of a decode error, matching the existing
   action dto's negative-case convention. */
dom_models_error_t pres_mqtt_handler_ir_tx_dto_decode(
    const char*                             data,
    int                                     data_len,
    pres_mqtt_handler_ir_tx_dto_request_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_IR_TX_DTO_H */
```

`main/src/presentation/mqtt/handler/ir_tx/dto.c`:

```c
#include "presentation/mqtt/handler/ir_tx/dto.h"

#include <string.h>

#include "cJSON.h"

dom_models_error_t pres_mqtt_handler_ir_tx_dto_decode(
    const char*                             data,
    int                                     data_len,
    pres_mqtt_handler_ir_tx_dto_request_t* out
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
    cJSON* raw_data_item     = cJSON_GetObjectItemCaseSensitive(json, "raw_data");

    if (cJSON_IsString(execution_id_item) && execution_id_item->valuestring) {
        strncpy(out->execution_id, execution_id_item->valuestring, sizeof(out->execution_id) - 1);
        out->execution_id_set = true;
    }

    if (cJSON_IsArray(raw_data_item)) {
        int    array_size = cJSON_GetArraySize(raw_data_item);
        size_t count       = 0;
        for (int i = 0; i < array_size && count < PRES_MQTT_HANDLER_IR_TX_DTO_RAW_DATA_MAX_LEN; i++) {
            cJSON* item = cJSON_GetArrayItem(raw_data_item, i);
            if (!cJSON_IsNumber(item)) {
                continue;
            }
            out->raw_data[count] = (int32_t)item->valuedouble;
            count++;
        }
        out->raw_data_count = count;
    }

    cJSON_Delete(json);

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 5: Write the `ir_tx` handler**

`main/include/presentation/mqtt/handler/ir_tx/handler.h`:

```c
#ifndef PRESENTATION_MQTT_HANDLER_IR_TX_HANDLER_H
#define PRESENTATION_MQTT_HANDLER_IR_TX_HANDLER_H

#include "presentation/mqtt/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void pres_mqtt_handler_ir_tx(pres_mqtt_context_t* ctx, const char* data, int data_len);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_IR_TX_HANDLER_H */
```

`main/src/presentation/mqtt/handler/ir_tx/handler.c`:

```c
#include "presentation/mqtt/handler/ir_tx/handler.h"

#include "domain/models/error.h"
#include "presentation/mqtt/handler/ir_tx/dto.h"

#define BASE_TAG "pres_mqtt_ir_tx"

#define IR_TX_ACK_STATUS_FAILED "FAILED"

void pres_mqtt_handler_ir_tx(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->info(ctx->logger, tag, "Received ir/tx request via MQTT");

    pres_mqtt_handler_ir_tx_dto_request_t request;
    dom_models_error_t                     err = pres_mqtt_handler_ir_tx_dto_decode(data, data_len, &request);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, tag, "Failed to parse ir/tx payload: %s (%d)", dom_models_error_str(err), (int)err);
        return;
    }

    if (!request.execution_id_set) {
        ctx->logger->error(ctx->logger, tag, "Invalid ir/tx payload: missing execution_id field");
        return;
    }

    if (request.raw_data_count == 0) {
        ctx->logger->error(ctx->logger, tag, "Invalid ir/tx payload: missing or empty raw_data field");
        (void)ctx->def_pub->ir_transmit_ack(ctx->def_pub, ctx->device_id_str, request.execution_id, IR_TX_ACK_STATUS_FAILED, "missing raw_data field");
        return;
    }

    if (!ctx->infrared) {
        ctx->logger->warn(ctx->logger, tag, "ir/tx received but IR is not wired on this build");
        (void)ctx->def_pub->ir_transmit_ack(ctx->def_pub, ctx->device_id_str, request.execution_id, IR_TX_ACK_STATUS_FAILED, "IR not supported on this device");
        return;
    }

    (void)ctx->infrared->transmit(ctx->infrared, request.execution_id, request.raw_data, request.raw_data_count);
}
```

- [ ] **Step 6: Route the `ir/tx` topic in `on_message.c`**

In `main/src/presentation/mqtt/event/on_message.c`, add `#include "presentation/mqtt/handler/ir_tx/handler.h"` and a new `else if` branch (after the `config_topic` branch, before the final `else`):

```c
    } else if (strcmp(ctx->topic_scratch, ctx->ir_tx_topic) == 0) {
        pres_mqtt_handler_ir_tx(ctx, event->data, event->data_len);
    } else {
```

- [ ] **Step 7: Update existing `pres_mqtt_context_new` call sites**

In `main/src/composition/main/presentation.c`, update the call:

```c
    launcher->presentation.mqtt_context = pres_mqtt_context_new(
        launcher->infrastructure.logger,
        launcher->infrastructure.preloaded_repository,
        launcher->application.messaging_callbacks,
        launcher->application.settings,
        launcher->application.ota,
        launcher->infrastructure.def_pub,
        NULL /* infrared: composition/main never wires IR, so it never
                subscribes to ir/tx either - see Task 6 */
    );
```

In `main/src/composition/counter_test/presentation.c`, the same change (that composition also uses `def_pub`, already present on its `launcher->infrastructure`):

```c
    launcher->presentation.mqtt_context = pres_mqtt_context_new(
        launcher->infrastructure.logger,
        launcher->infrastructure.preloaded_repository,
        launcher->application.messaging_callbacks,
        launcher->application.settings,
        launcher->application.ota,
        launcher->infrastructure.def_pub,
        NULL
    );
```

- [ ] **Step 8: Build**

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure && idf.py build
```
Expected: succeeds — this is the real test that both existing compositions (`main`, `counter_test`) still compile against the new required `pres_mqtt_context_new` signature.

- [ ] **Step 9: Format and commit**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
clang-format -i \
  main/include/presentation/mqtt/context.h main/src/presentation/mqtt/context.c \
  main/include/presentation/mqtt/context_utils.h main/src/presentation/mqtt/context_utils.c \
  main/src/presentation/mqtt/event/on_message.c \
  main/include/presentation/mqtt/handler/ir_tx/dto.h main/src/presentation/mqtt/handler/ir_tx/dto.c \
  main/include/presentation/mqtt/handler/ir_tx/handler.h main/src/presentation/mqtt/handler/ir_tx/handler.c \
  main/src/composition/main/presentation.c main/src/composition/counter_test/presentation.c
git diff --check
git add main/include/presentation/mqtt main/src/presentation/mqtt main/src/composition/main/presentation.c main/src/composition/counter_test/presentation.c
git commit -m "$(cat <<'EOF'
feat: (ir) route /sub/<device_id>/ir/tx to the infrared usecase

pres_mqtt_context_t gains a required def_pub and a nullable infrared
field; only builds that pass a non-null infrared and call its
subscribe() actually receive ir/tx messages. Replies FAILED
immediately if ir/tx somehow arrives with infrared unset (defensive -
composition/main never subscribes at all, see next commit).
EOF
)"
```

---

## Task 6: `composition/infrared` + `main.c`

**Files:**
- Create: `main/include/composition/infrared/{types,driver,infrastructure,application,presentation,launcher,utils,preloaded}.h`
- Create: `main/src/composition/infrared/{driver,infrastructure,application,presentation,launcher,utils,preloaded}.c`
- Modify: `main/main.c`

**Interfaces:**
- Consumes: everything produced by Tasks 2–5.
- Produces: `cmp_infrared_launcher(void)`, callable from `main.c` (not called by default — see Global Constraints).

- [ ] **Step 1: Duplicate `composition/main` wholesale**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
cp -r main/src/composition/main main/src/composition/infrared
cp -r main/include/composition/main main/include/composition/infrared
```

- [ ] **Step 2: Mechanically rename the copied identifiers**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
for f in main/src/composition/infrared/*.c main/include/composition/infrared/*.h; do
  sed -i \
    -e 's/cmp_main_/cmp_infrared_/g' \
    -e 's/COMPOSITION_MAIN_/COMPOSITION_INFRARED_/g' \
    -e 's#composition/main/#composition/infrared/#g' \
    "$f"
done
```
This renames every `cmp_main_*` function/type, every `COMPOSITION_MAIN_*` include guard, and every `#include "composition/main/...")` path inside the copied files to their `infrared` equivalents — including `cmp_main_launcher()` → `cmp_infrared_launcher()` and `cmp_main_launcher_t` → `cmp_infrared_launcher_t`.

- [ ] **Step 3: Add the `ir` field to infrastructure/application/presentation types**

In `main/include/composition/infrared/types.h`, add `#include "domain/contracts/device/ir.h"` and `#include "domain/usecases/internal/infrared.h"`. Add to `cmp_infrared_launcher_infrastructure_t`:

```c
    dom_contracts_device_ir_t*            ir;
```

Add to `cmp_infrared_launcher_application_t`:

```c
    dom_usecases_internal_infrared_t*     infrared;
```

- [ ] **Step 4: Wire the IR infrastructure in `infrastructure.c`**

In `main/src/composition/infrared/infrastructure.c`, add `#include "infrastructure/device/ir/esp_impl.h"`. In `cmp_infrared_infrastructure_init`, add (after the `wifi` init block, before the `esp_sntp_config_t` block):

```c
    launcher->infrastructure.ir = inf_device_ir_esp_impl_new(NULL);
    if (!launcher->infrastructure.ir) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    err = inf_device_ir_esp_impl_init(launcher->infrastructure.ir);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }
```
(`err` is already declared earlier in this function from the WiFi init step — reuse it, do not redeclare.)

In `cmp_infrared_infrastructure_deinit`, add (mirroring the `wifi` teardown block's position and shape):

```c
    if (launcher->infrastructure.ir) {
        inf_device_ir_esp_impl_deinit(launcher->infrastructure.ir);
        inf_device_ir_esp_impl_delete(launcher->infrastructure.ir);
        launcher->infrastructure.ir = NULL;
    }
```

- [ ] **Step 5: Wire the IR application usecase in `application.c`**

In `main/src/composition/infrared/application.c`, add `#include "application/internal/infrared/impl.h"`. In `cmp_infrared_application_init`, add (after the `messaging_callbacks` init block, at the end of the function before `return DOMAIN_MODELS_ERROR_OK;`):

```c
    app_internal_infrared_impl_cfg_t infrared_cfg = {
        .logger               = launcher->infrastructure.logger,
        .ir                   = launcher->infrastructure.ir,
        .def_pub              = launcher->infrastructure.def_pub,
        .def_sub              = launcher->infrastructure.def_sub,
        .preloaded_repository = launcher->infrastructure.preloaded_repository,
    };
    launcher->application.infrared = app_internal_infrared_impl_new(&infrared_cfg);
    if (!launcher->application.infrared) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    err = app_internal_infrared_impl_init(launcher->application.infrared);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }
```

In `cmp_infrared_application_deinit`, add (mirroring where `messaging_callbacks` teardown sits, before it since it was constructed after):

```c
    if (launcher->application.infrared) {
        app_internal_infrared_impl_deinit(launcher->application.infrared);
        app_internal_infrared_impl_delete(launcher->application.infrared);
        launcher->application.infrared = NULL;
    }
```

- [ ] **Step 6: Pass `def_pub`/`infrared` and subscribe in `presentation.c`**

In `main/src/composition/infrared/presentation.c`, update the `pres_mqtt_context_new` call:

```c
    launcher->presentation.mqtt_context = pres_mqtt_context_new(
        launcher->infrastructure.logger,
        launcher->infrastructure.preloaded_repository,
        launcher->application.messaging_callbacks,
        launcher->application.settings,
        launcher->application.ota,
        launcher->infrastructure.def_pub,
        launcher->application.infrared
    );
```

Immediately after the existing `pres_mqtt_context_init(...)` success check (before the `wifi_sta_reconnect_task` block), add the subscribe call — this is what makes `composition/infrared` the only build variant that actually receives `ir/tx` messages:

```c
    err = launcher->application.infrared->subscribe(launcher->application.infrared);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }
```

- [ ] **Step 7: Add the commented-out launcher option to `main.c`**

In `main/main.c`:

```c
// #include "composition/counter_test/launcher.h"
// #include "composition/infrared/launcher.h"
#include "composition/main/launcher.h"
// #include "composition/test_seed/seed.h"

void app_main(void) {
    // cmp_test_seed_run();
    // cmp_counter_test_launcher();
    // cmp_infrared_launcher();
    cmp_main_launcher();
}
```

- [ ] **Step 8: Build with `composition/main` active (the shipped state)**

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure && idf.py build
```
Expected: succeeds. `composition/infrared`'s `.c` files still get compiled (they're under `main/src`, unconditionally globbed) even though `cmp_infrared_launcher()` isn't called — this build must be clean regardless of which launcher `main.c` activates.

- [ ] **Step 9: Temporarily activate `composition/infrared` and build/flash/monitor**

Edit `main/main.c` locally (do not commit this state):

```c
#include "composition/infrared/launcher.h"
// #include "composition/main/launcher.h"
...
    cmp_infrared_launcher();
    // cmp_main_launcher();
```

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure && idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
```
Watch for a bounded window (e.g. 60s) confirming: clean boot, no reset loop, no stack-protection fault, WiFi/MQTT/BLE come up the same as `composition/main` does today, and no IR-specific error logged during `inf_device_ir_esp_impl_init`/`app_internal_infrared_impl_init`. Exit the monitor cleanly (Ctrl+]).

- [ ] **Step 10: Revert `main.c` to the shipped state and rebuild**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
git checkout -- main/main.c
```
Re-apply Step 7's edit if `git checkout` reverted past it too (it will, since Step 7 wasn't committed yet) — redo Step 7's edit exactly, this time to keep as the committed state, then:

```bash
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
idf.py reconfigure && idf.py build
```
Expected: succeeds with `composition/main` active again, per the design decision.

- [ ] **Step 11: Format and commit**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
clang-format -i main/src/composition/infrared/*.c main/include/composition/infrared/*.h main/main.c
git diff --check
git add main/src/composition/infrared main/include/composition/infrared main/main.c
git commit -m "$(cat <<'EOF'
feat: (ir) add composition/infrared build variant

Full duplicate of composition/main with IR infrastructure/application
wired in and ir/tx actually subscribed - the only composition that
does. Added to main.c as a commented-out alternative; composition/main
stays the active launcher.
EOF
)"
```

---

## Task 7: Document the IR contract and build variant in the root `AGENTS.md`

**Files:**
- Modify: `AGENTS.md`

**Interfaces:** None (documentation only).

- [ ] **Step 1: Add the three new topics to the "Protocol contracts" → MQTT section**

In `AGENTS.md`, in the bulleted list under `### MQTT` that currently ends with `/sub/<device_id>/config`, add:

```markdown
  - `/sub/<device_id>/ir/tx` (only subscribed by the `composition/infrared`
    build variant)
- Publish IR captures on `/pub/<device_id>/ir/rx` and transmit results on
  `/pub/<device_id>/ir/tx_ack`, both only from `composition/infrared`.
```

- [ ] **Step 2: Document `composition/infrared` alongside the other build variants**

In the `### Firmware layers` section's description of `composition/main/`, `composition/test_seed/`, add a new bullet:

```markdown
- **`composition/infrared/`** — alternate boot composition, a full
  duplicate of `composition/main/` with `infrastructure/device/ir` and
  `application/internal/infrared` additionally wired in. The only
  composition that subscribes to `ir/tx`. Selected the same way as
  `composition/counter_test/` — by editing the include/call in
  `main/main.c` — not by Kconfig. `composition/main` is the default active
  launcher; `composition/infrared` ships commented out until a build is
  explicitly targeting IR-capable hardware.
```

- [ ] **Step 3: Commit**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
git add AGENTS.md
git commit -m "docs: document ir/rx, ir/tx, ir/tx_ack topics and composition/infrared"
```

---

## Task 8: `mate-things` — fix `extractTopicParts` for nested topic suffixes

The current parser (`backend/internal/presentation/mqtt/event/message.go`) only
accepts exactly 2 or 3 `/`-separated segments. `/pub/{device_id}/ir/rx` has
4 segments and would silently fail to route today. This must be fixed
**before** Task 9 renames the topics, or the rename introduces a routing
regression.

**Files:**
- Modify: `backend/internal/presentation/mqtt/event/message.go`
- Test: `backend/internal/presentation/mqtt/event/message_test.go` (new)

**Interfaces:**
- Produces (consumed by Task 9): `extractTopicParts(topic string) (topicParts, bool)`, unchanged signature, `Suffix` now supports multi-segment values like `"ir/rx"`.

- [ ] **Step 1: Write the failing test**

Create `backend/internal/presentation/mqtt/event/message_test.go`:

```go
package presentationmqttevent

import "testing"

func TestExtractTopicPartsHandlesNestedSuffix(t *testing.T) {
	got, ok := extractTopicParts("/pub/ABCDEF012345/ir/rx")
	if !ok {
		t.Fatalf("expected ok=true, got false")
	}
	want := topicParts{Direction: "pub", DeviceId: "ABCDEF012345", Suffix: "ir/rx"}
	if got != want {
		t.Fatalf("got %+v, want %+v", got, want)
	}
}

func TestExtractTopicPartsHandlesFlatSuffix(t *testing.T) {
	got, ok := extractTopicParts("/pub/ABCDEF012345/status")
	if !ok {
		t.Fatalf("expected ok=true, got false")
	}
	want := topicParts{Direction: "pub", DeviceId: "ABCDEF012345", Suffix: "status"}
	if got != want {
		t.Fatalf("got %+v, want %+v", got, want)
	}
}

func TestExtractTopicPartsHandlesGlobalSuffix(t *testing.T) {
	got, ok := extractTopicParts("/pub/registration")
	if !ok {
		t.Fatalf("expected ok=true, got false")
	}
	want := topicParts{Direction: "pub", Suffix: "registration", IsGlobal: true}
	if got != want {
		t.Fatalf("got %+v, want %+v", got, want)
	}
}

func TestExtractTopicPartsRejectsEmptyNestedSegment(t *testing.T) {
	if _, ok := extractTopicParts("/pub/ABCDEF012345/ir//rx"); ok {
		t.Fatalf("expected ok=false for empty inner segment")
	}
}

func TestExtractTopicPartsRejectsTooFewSegments(t *testing.T) {
	if _, ok := extractTopicParts("/pub"); ok {
		t.Fatalf("expected ok=false for a single segment")
	}
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go test ./internal/presentation/mqtt/event/... -run TestExtractTopicParts -v
```
Expected: `TestExtractTopicPartsHandlesNestedSuffix` FAILs (`ok=false`, current code's `default:` case rejects 4-segment topics). The other four should already pass against the current implementation.

- [ ] **Step 3: Fix `extractTopicParts`**

In `backend/internal/presentation/mqtt/event/message.go`, replace the `switch len(parts) { ... }` block inside `extractTopicParts` with:

```go
	switch {
	case len(parts) == 2:
		if parts[0] == "" || parts[1] == "" {
			return topicParts{}, false
		}
		return topicParts{
			Direction: parts[0],
			Suffix:    parts[1],
			IsGlobal:  true,
		}, true

	case len(parts) >= 3:
		if parts[0] == "" || parts[1] == "" {
			return topicParts{}, false
		}
		for _, segment := range parts[2:] {
			if segment == "" {
				return topicParts{}, false
			}
		}
		return topicParts{
			Direction: parts[0],
			DeviceId:  parts[1],
			Suffix:    strings.Join(parts[2:], "/"),
		}, true

	default:
		return topicParts{}, false
	}
```

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go test ./internal/presentation/mqtt/event/... -v
```
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
cd /home/dodol/Repositories/mate/mate-things
git add backend/internal/presentation/mqtt/event/message.go backend/internal/presentation/mqtt/event/message_test.go
git commit -m "$(cat <<'EOF'
fix: (mqtt) support nested topic suffixes in extractTopicParts

3-segment-only parsing silently dropped any /pub/<device_id>/<a>/<b>
topic. Needed before the ir/rx, ir/tx_ack rename (both nested) lands.
EOF
)"
```

---

## Task 9: `mate-things` — rename `ir_capture`/`ir_transmit`/`ir_transmit_ack` topics

**Files:**
- Modify: `backend/internal/infrastructure/node/subscriptions/mqtt.go`
- Modify: `backend/internal/infrastructure/node/publish/mqtt.go`
- Modify: `backend/internal/presentation/mqtt/event/message.go`

**Interfaces:** None new — topic string literals only. `IrCapture`, `IrTransmitAck`, `IrTransmitPayload` DTOs and all handler/usecase signatures are unchanged.

- [ ] **Step 1: Rename the subscribed suffixes and their QoS constants**

In `backend/internal/infrastructure/node/subscriptions/mqtt.go`, rename the two consts and update the two methods:

```go
	irRxQos    = 1
	irTxAckQos = 1
```
(replacing `irCaptureQos`/`irTransmitAckQos`)

```go
func (m *mqttImpl) IrCapture(ctx context.Context, nodeDeviceId string) (err error) {
	return m.subscribe(ctx, infrastructurenodeshared.NodePubTopic(nodeDeviceId, "ir/rx"), irRxQos)
}

func (m *mqttImpl) IrTransmitAck(ctx context.Context, nodeDeviceId string) (err error) {
	return m.subscribe(ctx, infrastructurenodeshared.NodePubTopic(nodeDeviceId, "ir/tx_ack"), irTxAckQos)
}
```
(method names stay `IrCapture`/`IrTransmitAck` — they implement the
`domaincontractsnode.Subscriptions` interface, whose method names are not
part of this rename's scope; only the wire topic strings and local QoS
const names change.)

- [ ] **Step 2: Rename the published suffix and its QoS constant**

In `backend/internal/infrastructure/node/publish/mqtt.go`, rename the const:

```go
	irTxQos = 1
```
(replacing `irTransmitQos`)

Update `IrTransmit`'s `m.publish(...)` call:

```go
	return m.publish(
		ctx, infrastructurenodeshared.NodeSubTopic(nodeDeviceId, "ir/tx"),
		irTxQos, false, payload,
	)
```

- [ ] **Step 3: Update the inbound topic-suffix switch**

In `backend/internal/presentation/mqtt/event/message.go`'s `OnMessage`, update the two `case` labels:

```go
		case "ir/rx":
			h.IrCapture(ctx, msg, topic.DeviceId)
		case "ir/tx_ack":
			h.IrTransmitAck(ctx, msg, topic.DeviceId)
```

- [ ] **Step 4: Build and test**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go build ./...
go test ./...
```
Expected: both succeed. No existing test asserted the old literal topic
strings (confirmed via `grep -rln "ir_capture\|ir_transmit" backend/internal --include=*_test.go` returning nothing before this change) — Task 8's new tests exercise the parser with the new-shaped topics already.

- [ ] **Step 5: Commit**

```bash
cd /home/dodol/Repositories/mate/mate-things
git add backend/internal/infrastructure/node/subscriptions/mqtt.go backend/internal/infrastructure/node/publish/mqtt.go backend/internal/presentation/mqtt/event/message.go
git commit -m "$(cat <<'EOF'
feat: (mqtt) rename ir_capture/ir_transmit/ir_transmit_ack topics to
ir/rx, ir/tx, ir/tx_ack

Matches the mate-espidf-base firmware side. Payload shapes (IrCapture,
IrTransmitPayload, IrTransmitAck DTOs) are unchanged - topic strings
only.
EOF
)"
```

---

## Task 10: End-to-end verification

**Files:** none (verification only — a throwaway local script is fine, do not commit it).

- [ ] **Step 1: Confirm both repos build clean from a fresh checkout state**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
source /home/dodol/.espressif/tools/activate_idf_v6.0.2.sh
idf.py reconfigure && idf.py build

cd /home/dodol/Repositories/mate/mate-things/backend
go build ./... && go test ./...
```
Expected: both succeed with no warnings/failures.

- [ ] **Step 2: Round-trip the renamed contract with a dummy MQTT node**

Using the same `paho-mqtt` 1.6.1-based dummy-node pattern established during
the earlier cooperative Infrared Record Wizard testing session against this
project's real HiveMQ Cloud broker (credentials via `BE_MQTT_HOST` /
`BE_MQTT_PASSWORD` etc., not the local `development-emqx` container):

1. Connect a dummy node client, publish a registration on
   `/pub/registration` with a real 12-hex-char `device_id`, matching
   `node_class_name` already seeded in `node_classes`.
2. Subscribe the dummy node to `/sub/{device_id}/ir/tx`.
3. From the backend (via a real Infrared Record Wizard test-case transmit,
   or a direct call to the `IrTransmit` publish method in a throwaway
   script), publish an `ir/tx` command and confirm the dummy node receives
   it on the new topic with the unchanged `{"execution_id", "raw_data"}`
   shape.
4. From the dummy node, publish a synthetic `{"raw_data": [9000, 4500, ...]}`
   payload on `/pub/{device_id}/ir/rx` and confirm the backend's
   `CaptureIrRaw` usecase picks it up (check via the record session's case
   list reflecting a new raw capture, or backend logs showing
   `ir_capture`/`CaptureIrRaw` processing without a topic-parse warning).
5. From the dummy node, publish `{"execution_id": "...", "status":
   "SUCCESS"}` on `/pub/{device_id}/ir/tx_ack` and confirm the backend logs
   `"ir transmit acknowledged"` (per
   `presentation/mqtt/handler/ir_transmit_ack.go`'s existing debug log) with
   no topic-parse warning.

Expected: all four messages route correctly end-to-end with no "invalid mqtt
topic" or "unknown mqtt topic" warnings in the backend logs — confirming
Task 8's parser fix and Task 9's rename are consistent with each other and
with the firmware side built in Tasks 1–6.

- [ ] **Step 3: Report results**

No commit for this task — it's a verification gate. If any step fails,
return to the relevant earlier task to fix it before considering the
feature done.
