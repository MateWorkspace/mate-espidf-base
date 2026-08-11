# IR Firmware Implementation Design

## Context

`mate-things` already has a full backend-side infrared record/replay pipeline
(the "Spec B" Infrared Record Wizard). It talks to nodes over MQTT using
`/pub/{device_id}/ir_capture`, `/sub/{device_id}/ir_transmit`, and
`/pub/{device_id}/ir_transmit_ack`. No node in this repo (`mate-espidf-base`)
implements the device side of that contract yet — it was only ever exercised
against a scripted dummy node during backend testing.

A previous project, `smart-IR-embedded`, has a working, hardware-validated
`components/infrared` component (GPIO-ISR + `esp_timer` idle-timeout RX,
RMT-based TX) and a `main/` composition that wires it through a Clean
Architecture stack similar to this repo's. This design ports that component
into `mate-espidf-base` and extends this repo's own layered architecture
(`domain/contracts` → `domain/usecases/internal` → `application/internal` →
`infrastructure` → `presentation/mqtt` → `composition`) to drive it, while
renaming the MQTT topics involved on both sides of the contract.

## Topic rename (cross-repo wire contract)

| Old topic | New topic | Direction | Payload (unchanged) |
|---|---|---|---|
| `/pub/{device_id}/ir_capture` | `/pub/{device_id}/ir/rx` | node → backend | `{"raw_data": [int32, ...]}` |
| `/sub/{device_id}/ir_transmit` | `/sub/{device_id}/ir/tx` | backend → node | `{"execution_id": "<uuid>", "raw_data": [int32, ...]}` |
| `/pub/{device_id}/ir_transmit_ack` | `/pub/{device_id}/ir/tx_ack` | node → backend | `{"execution_id": "<uuid>", "status": "SUCCESS"\|"FAILED", "message": "..."}` |

`raw_data` is an alternating mark/space sequence of microsecond durations,
always starting with a mark (index 0). No DTO/payload shape changes in
either repo — only topic string literals move. Both repos must land their
half of this rename together or they stop being able to talk to each other.

## `mate-espidf-base` changes

### 1. Ported `infrared` ESP-IDF component

Copy `smart-IR-embedded/components/infrared/` to
`mate-espidf-base/components/infrared/` near-verbatim:

- `include/infrared.h`, `include/infrared_types.h`, `include/infrared_rx.h`,
  `include/infrared_tx.h`
- `src/infrared.c`, `src/infrared_internal.h`, `src/infrared_rx.c`,
  `src/infrared_tx.c`
- `CMakeLists.txt` (`REQUIRES driver esp_driver_rmt esp_timer freertos`)
- `AGENTS.md`, carried over and adapted: trimmed of "not yet implemented"
  roadmap/priorities framing, updated to describe the component as already
  implemented (ported from `smart-IR-embedded`), otherwise keeping its
  API-shape, lifecycle, ISR-safety, and testing-checklist documentation as
  the authoritative reference for this component specifically (this repo's
  established pattern of "everything lives in the root AGENTS.md" is broken
  intentionally here, matching the source project's own convention of a
  component-local doc for a self-contained peripheral driver).

No functional changes: same `_new/_init/_deinit/_delete` lifecycle, same
`infrared_cfg_t` fields (`rx_gpio`, `tx_gpio`, `enable_rx`, `enable_tx`,
`on_receive`, `user_ctx`, RX task/queue tuning), same
`INFRARED_CFG_DEFAULT()`, same RX (GPIO ISR + idle-timeout finalization,
20000us default, 512 max durations) and TX (RMT copy encoder, 38kHz/33%
carrier) internals. This component knows nothing about `dom_models_error_t`
or MQTT — it stays ESP-IDF-native (`esp_err_t`,
`infrared_duration_t{bool level; uint32_t duration_us;}`), same boundary
`smart-IR-embedded/main/infrastructure/ir/control/infrared_impl.c` draws
today.

### 2. Domain layer

**`main/include/domain/models/ir.h`** (new):
```c
typedef struct {
    bool     level;
    uint32_t duration_us;
} dom_models_ir_duration_t;

typedef void (*dom_models_ir_receive_cb_t)(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    void*                           cb_ctx
);
```

**`main/include/domain/contracts/device/ir.h`** (new), following the shape
of `domain/contracts/device/wifi.h`:
```c
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
```
Plus the standard `dom_contracts_device_ir_new`/`_delete` inline helpers,
matching every other contract header in this tree.

**`main/include/domain/usecases/internal/infrared.h`** (new), alongside
`wifi_manager.h`/`messaging_callbacks.h`:
```c
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
```
`subscribe` issues the `/sub/{device_id}/ir/tx` MQTT subscription (called
once from `composition/infrared`'s presentation stage, never from
`composition/main`). `transmit` is invoked by the presentation-layer MQTT
handler when an `ir/tx` message arrives; it rebuilds `(level, duration)`
pairs from the flat `raw_data` array (mark, space, mark, ... — `level=true`
for index 0, alternating), calls the injected `dom_contracts_device_ir_t`'s
`transmit`, and publishes `ir/tx_ack` with `SUCCESS`/`FAILED` based on the
result — the same ack-after-attempt pattern
`app_internal_messaging_callbacks_impl.c` already uses for action acks.

RX has no method on this interface — it's push-based, not
presentation-triggered. At `_init` time the usecase registers itself as the
`dom_contracts_device_ir_t` receive handler; when a frame arrives, the
callback strips the `level` field (the component's ISR already guarantees
index 0 is the leading mark, so the flat array reconstructs unambiguously)
and calls `def_pub->ir_capture(...)` directly from that callback context —
the same "publish straight from a registered callback" pattern
`on_log_message` in `messaging_callbacks_impl.c` already uses for log
forwarding.

### 3. Extending `domain/contracts/messaging/def_pub.h` / `def_sub.h`

This is the part originally scoped as "extend the messaging contracts":

`def_pub.h` gains two members on `dom_contracts_messaging_def_pub_t`:
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

`def_sub.h` gains one member on `dom_contracts_messaging_def_sub_t`:
```c
dom_models_error_t (*ir_tx)(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
);
```

Both `infrastructure/messaging/def_pub/mqtt_impl.c` and
`infrastructure/messaging/def_sub/mqtt_impl.c` get corresponding
implementations publishing/subscribing on the Section "Topic rename" topics,
following the exact pattern their sibling methods (`action_ack`, `ota`, etc.)
already use for topic-string building and QoS. The existing
`stub_impl.c` files for both gain matching no-op stub implementations (every
existing method has one; these are not optional).

### 4. Infrastructure: `infrastructure/device/ir/esp_impl.{h,c}`

`inf_device_ir_esp_impl_*`, wrapping the ported component:
- Config struct holds `rx_gpio`/`tx_gpio` as hardcoded `#define` constants
  local to this file (per project decision: no Kconfig for these — a future
  hardware revision needing different pins is a code change + rebuild, same
  tradeoff already accepted for e.g. NTP server / task priorities elsewhere
  in this repo).
- `_new` builds an `infrared_cfg_t` via `INFRARED_CFG_DEFAULT()` with both
  RX and TX enabled, GPIOs set from the constants.
- `_init` calls `infrared_init`; the registered `on_receive` trampoline
  converts `infrared_rx_frame_t` → `dom_models_ir_duration_t[]` and invokes
  whatever `dom_models_ir_receive_cb_t` was registered via
  `set_receive_handler` (mirrors `smart-IR-embedded`'s
  `inf_ir_control_infrared_impl_on_receive` closely — same overflow-drop and
  null-checks).
- `transmit` converts `dom_models_ir_duration_t[]` → `infrared_duration_t[]`
  and calls `infrared_transmit`; maps `esp_err_t` → `dom_models_error_t`
  through the same switch shape used elsewhere in this repo's infra layer
  (`ESP_ERR_INVALID_ARG` → `DOMAIN_MODELS_ERROR_BAD_ARGUMENT`,
  `ESP_ERR_NO_MEM` → `DOMAIN_MODELS_ERROR_MALLOC_FAILED`, etc.).
- `_deinit`/`_delete` call `infrared_deinit`/`infrared_delete` on the owned
  handle.

### 5. Application: `application/internal/infrared/impl.{h,c}`

`app_internal_infrared_impl_*`, config `{logger, ir, def_pub, def_sub,
preloaded_repository}` (same config-struct/`impl_types.h`/`impl_utils.h`
split as every other `application/internal/*` usecase):

- `_init`: loads `device_id_str` from `preloaded_repository`, calls
  `ir->set_receive_handler(ir, ctx, on_ir_receive)`.
- `on_ir_receive` (static, registered callback): strips `level`, calls
  `def_pub->ir_capture(def_pub, device_id_str, raw_data, count)`.
- `subscribe_impl`: calls `def_sub->ir_tx(device_id_str, def_sub)`.
- `transmit_impl(execution_id, raw_data, raw_data_count)`: rebuilds
  `dom_models_ir_duration_t[]` from the flat array, calls `ir->transmit`,
  publishes `ir_transmit_ack` with `"SUCCESS"` or `"FAILED"` + an error
  message on failure.

### 6. Presentation: MQTT wiring + graceful "not wired" handling

`pres_mqtt_context_t` (`presentation/mqtt/context.h`) gains two new fields:
- `dom_contracts_messaging_def_pub_t* def_pub` — **required** (new
  constructor parameter). Needed so the `ir/tx` handler can always publish a
  `FAILED` ack even in the extremely unlikely case `infrared` is somehow
  null on a build that did subscribe (defensive; see below).
- `dom_usecases_internal_infrared_t* infrared` — **nullable** (new optional
  constructor parameter, `NULL` on `composition/main`, the real usecase on
  `composition/infrared`).

`pres_mqtt_context_t` also gains `ir_tx_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN]`,
built in `pres_mqtt_context_init` as `/sub/{device_id}/ir/tx` — but this
topic is only ever subscribed to by `composition/infrared` (see Section 7);
`composition/main` builds the string but never subscribes to it, so it's
inert there.

**`presentation/mqtt/handler/ir_tx/dto.{h,c}`** (new, folder-per-handler
matching `action`/`config`/`ota`): parses `{"execution_id": string,
"raw_data": [int32, ...]}`.

**`presentation/mqtt/handler/ir_tx/handler.{h,c}`** (new):
```c
void pres_mqtt_handler_ir_tx(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    // decode dto -> request{execution_id, raw_data[], raw_data_count}
    // on decode failure: log + return (matches action/config handler behavior)
    if (!ctx->infrared) {
        ctx->logger->warn(ctx->logger, tag, "ir/tx received but IR is not wired on this build");
        (void)ctx->def_pub->ir_transmit_ack(ctx->def_pub, ctx->device_id_str, request.execution_id, "FAILED", "IR not supported on this device");
        return;
    }
    (void)ctx->infrared->transmit(ctx->infrared, request.execution_id, request.raw_data, request.raw_data_count);
}
```
`presentation/mqtt/event/on_message.c` adds an `else if
(strcmp(ctx->topic_scratch, ctx->ir_tx_topic) == 0)` branch calling this
handler.

No new presentation-layer handler for `ir/rx` — RX publishing happens from
inside `application/internal/infrared/impl.c`'s registered receive callback
(Section 5), not from anything arriving over MQTT to route.

### 7. `composition/infrared` (new, parallel to `composition/main`)

Duplicate `composition/main/{driver,infrastructure,application,presentation,
preloaded,utils,launcher}.{h,c}` and `types.h` into a new
`composition/infrared/` tree (`cmp_infrared_*` naming), identical to
`composition/main` in every respect (WiFi, MQTT, BLE, OTA, settings, log
forwarding, messaging_callbacks — IR nodes still need the full base feature
set) except:

- `cmp_infrared_launcher_infrastructure_t` gains `dom_contracts_device_ir_t* ir;`.
- `infrastructure.c` additionally constructs/initializes it via
  `inf_device_ir_esp_impl_new`/`_init`.
- `cmp_infrared_launcher_application_t` gains
  `dom_usecases_internal_infrared_t* infrared;`.
- `application.c` additionally constructs/initializes it via
  `app_internal_infrared_impl_new`/`_init`, wired to
  `launcher->infrastructure.ir`.
- `presentation.c`'s `pres_mqtt_context_new(...)` call passes
  `launcher->infrastructure.def_pub` and `launcher->application.infrared`
  (vs. `composition/main` passing `NULL` for the `infrared` parameter and
  still needing to pass `def_pub` now that it's a required parameter there
  too). After context init, it additionally calls
  `launcher->application.infrared->subscribe(launcher->application.infrared)`
  — this is the "only `composition/infrared` ever subscribes to `ir/tx`"
  gate: `composition/main` never calls `subscribe`, so it never receives
  `ir/tx` messages at all, and the nil-guard in the handler becomes purely
  defensive (only reachable if `composition/infrared` itself somehow ends up
  with the subscription active but the usecase pointer null, which
  shouldn't happen given `_init` ordering, but costs nothing to guard).
- Deinit order mirrors `composition/main`'s reverse-of-init pattern with the
  IR infra/application entries added at the correct position (after
  `wifi`/before `def_pub` in infra deinit, matching where its `_new` sits
  in init).

### 8. `main.c`

`composition/main` stays the active (uncommented) launcher, per your
decision — `composition/infrared` is added to `main.c` as a commented-out
alternative, matching the existing `counter_test`/`test_seed` selection
pattern:
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

## `mate-things` backend changes

Purely mechanical topic-string renames, no DTO/logic changes:

- `backend/internal/infrastructure/node/subscriptions/mqtt.go`:
  `"ir_capture"` → `"ir/rx"`, `"ir_transmit_ack"` → `"ir/tx_ack"`.
- `backend/internal/infrastructure/node/publish/mqtt.go`: `"ir_transmit"` →
  `"ir/tx"`.
- `backend/internal/presentation/mqtt/event/message.go`: update the
  topic-suffix `switch` cases (`"ir_capture"` → matching the new suffix,
  `"ir_transmit_ack"` → matching the new suffix) to route correctly. Exact
  string form depends on how deep the existing topic-suffix splitting goes
  (single suffix segment vs. further split on `/`) — confirmed during
  implementation by reading `message.go`'s topic-parsing code directly, not
  assumed here.
- No changes to `presentation/mqtt/dto/*.go`, `handler/ir_capture.go`,
  `handler/ir_transmit_ack.go`, or any usecase/domain code.

## Verification

- **mate-espidf-base**: `idf.py reconfigure && idf.py build` with
  `composition/main` active (must build clean with the new component/
  contracts compiled in but unused by the active launcher). Temporarily flip
  `main.c` to `composition/infrared` locally, confirm clean build, flash to
  `/dev/ttyACM0`, bounded monitor for a clean boot (no reset loop, no
  stack-protection fault), following `docs/agent_test/` conventions. Format
  touched files with `clang-format`; `git diff --check` clean. Revert
  `main.c` back to `composition/main` active before considering the branch
  done, per the approved decision.
- **mate-things**: `go build ./...`, `go test ./...`; grep existing
  subscription/publish tests for literal topic-string assertions and update
  them to match.
- **End-to-end**: reuse the dummy-node MQTT test harness from the earlier
  cooperative Infrared Record Wizard testing session, updated to
  publish/subscribe on `ir/rx`, `ir/tx`, `ir/tx_ack`, to confirm the renamed
  contract still round-trips against the updated backend before any real
  hardware test.
