# Architecture Consistency Refactor — Design

## Context

`mate-espidf-base` is a layered firmware codebase (`domain / application /
presentation / infrastructure / composition`, mirrored under `main/include`
and `main/src`). The BLE transport (settings, wifi_manager, log services)
was added and hardened this session — including fixing two real crash bugs
caused by stack-local buffers in GATT callbacks (see
`docs/agent_test/v1.0.0-dev.1/scenario/09-known-gaps-summary.md`, bugs
#6/#7). While fixing those, the repo owner noticed the BLE and MQTT
transports don't follow the same conventions for equivalent concerns, and
asked for a full audit + refactor plan to make the codebase consistent and
easier for a human to maintain and debug.

A full audit (general-purpose agent, evidence-based, file:line references)
found 10 categories of inconsistency, summarized below. Full detail with
exact citations is in the audit transcript; this spec captures the
decisions made from it, not the raw findings.

## Goal

Establish one canonical pattern per architectural concern (lifecycle,
log-forwarding layering, DTO placement, buffer ownership, logging tags,
cfg validation, error-logging responsibility, callback-overflow handling)
and bring every module in the codebase into line with it — including
retrofitting the MQTT transport, which was the primary source of drift
since BLE was built more recently and more carefully.

Non-goals: no new features, no behavior changes visible to the backend or
over the wire (MQTT/BLE payload shapes stay identical), no changes to the
domain layer's data model.

## Canonical patterns

### 1. Module lifecycle: `_new` / `_init` / `_deinit` / `_delete`

This is BLE's existing shape, chosen as canonical because it cleanly
separates "object exists" from "object is doing its job," which matters
for idempotency and for reasoning about what state a module is in during
startup/shutdown ordering in `composition/main`.

- **`_new(cfg)`**: validate `cfg` (via a `validate_cfg()` helper — see
  canonical pattern #7), `calloc` the struct, `memcpy`/assign `cfg` into
  `self->cfg`. No side effects — no registration, no callback wiring, no
  I/O. Returns `NULL` on any failure, logging nothing itself (the caller's
  own `_new` composition step logs).
- **`_init(self)`**: idempotent — checks a `self->registered` (or
  equivalent) flag and returns `DOMAIN_MODELS_ERROR_OK` immediately if
  already initialized. Does the actual wiring: GATT service registration,
  callback subscription, `esp_mqtt_client_register_event`, etc. Logs
  exactly one `logger->info(..., "<Module> initialized successfully")`
  line on success.
- **`_deinit(self)`**: symmetric teardown, guarded the same way, unwinds
  everything `_init` set up (unsubscribe callbacks, disable handlers).
  Silent on success (matches existing BLE deinit behavior).
- **`_delete(self)`**: calls `_deinit(self)`, then `free(self)`.

Every `application/internal/*` usecase (`settings`, `ota`, `wifi_manager`,
`messaging_callbacks`) and `presentation/mqtt/context` currently do
validation + allocation + wiring all inside `_new`, with no `_init`/
`_deinit` pair and no idempotency guard. They gain the split; behavior at
call sites changes only in that `composition/main/*.c` must now call
`_init()` after `_new()` for these modules (mirroring how it already calls
`pres_ble_handler_settings_init()` etc.).

### 2. Log-forwarding layering: a shared `log_forwarding` usecase

Today MQTT wires log-forwarding through an application-layer usecase
(`messaging_callbacks`), while BLE's log handler subscribes directly to
`dom_contracts_logger_leveled_t` from the presentation layer. The usecase
route is chosen as canonical (it matches the domain→application→
presentation layering used everywhere else, and is the shape a third
transport should copy).

A new `domain/usecases/internal/log_forwarding` module owns the logger
subscription lifecycle (`_new`/`_init`/`_deinit`/`_delete`, per pattern
#1) and exposes a transport-agnostic way to register a per-transport sink
(a function pointer taking the formatted log line + length). MQTT's
`messaging_callbacks` and BLE's `log` handler both become *consumers* of
this usecase instead of one of them owning the subscription and the other
reaching around it. The usecase itself still just forwards raw lines —
no transport framing, no queuing behavior change from what BLE's handler
already does (that queue/task stays presentation-side, since it's BLE-
specific buffering for GATT notification, not a domain concern).

### 3. DTO placement: every transport handler that parses/builds a wire
payload gets a `dto.c`/`.h` pair

Matches BLE's existing `settings/dto.c` and `wifi_manager/dto.c`. MQTT's
`presentation/mqtt/handler/action.c` and `ota.c` get
`action/dto.c`+`.h` and `ota/dto.c`+`.h` (restructured into per-handler
subdirectories to match BLE's `handler/<name>/{handler,dto,types}.{c,h}`
layout, since MQTT currently has flat `handler/action.c` instead of
`handler/action/handler.c`). `registration_ack.c` does no payload parsing
(it only reacts to receipt), so it gets no DTO file — this is intentional,
not a gap.

### 4. Buffer ownership: no meaningful stack-local buffers in
task-shared/shallow-stack callbacks

`presentation/mqtt/event/on_message.c` currently stack-allocates `topic`
(128B) + three 64B "expected topic" buffers on every MQTT event-task
invocation. These move to `pres_mqtt_context_t` as struct-owned fields
(mirroring the `types.h` pattern BLE's settings/wifi_manager handlers now
use), with named `#define ..._MAX_LEN` constants and the same "why" comment
explaining the stack-overflow precedent from earlier this session.

### 5. Logging tag convention: `BASE_TAG` + per-function suffix, everywhere

`#define BASE_TAG "<module/path>"` at file scope; each function builds its
own `const char* tag = BASE_TAG "/<function_name>";`. This already covers
`application/internal/*` and all 3 BLE handlers. `presentation/ble/host.c`
renames its lone `TAG_PATH` macro to `BASE_TAG` (same value, name only).
Every MQTT presentation file (`event/on_connect.c`, `on_disconnect.c`,
`on_error.c`, `on_message.c`, `handler/action.c`, `handler/ota.c`,
`handler/registration_ack.c`) and `composition/main/launcher.c`,
`composition/test_seed/seed.c` convert their flat `TAG` (used as-is in
every log call with no per-function distinction) into `BASE_TAG` with
per-function suffixes added at each log call site.

### 6. Cfg validation: dedicated `validate_cfg()` helper, everywhere

Matches the existing `application/internal/*/impl_utils.c` and
`infrastructure/*/impl_utils.c` pattern. The 4 BLE modules
(`log`, `settings`, `wifi_manager` handlers, `ble/host`) and
`presentation/mqtt/context` currently inline their null checks directly in
`_new()`; each gains a small `*_utils.c`/`.h` (or an addition to an
existing one, e.g. `ble/host_utils.c` if that doesn't already exist) with
a `..._validate_cfg(const ..._cfg_t* cfg)` function, called first thing in
`_new()`. No behavior change — same checks, just extracted.

### 7. Error-logging responsibility: log once, at the usecase

The usecase/application layer is the single source of truth for "this
operation failed" — it already logs via `logger->error(...)` at the point
of failure. Presentation handlers translate the returned
`dom_models_error_t` into a transport-appropriate response (BLE ATT error
code, MQTT silently-no-ack, etc.) but do not re-log the same failure.
`presentation/mqtt/handler/ota.c` and `action.c` currently duplicate the
usecase's log line (with slightly different wording) on the way out —
these redundant `logger->error` calls are removed, matching what BLE's
handlers already do (they log nothing on a usecase-reported failure,
trusting the usecase's line).

### 8. Callback-array overflow: always a hard error

`dom_contracts_logger_leveled_t.add_callback` currently returns `void` and
silently drops the registration if the fixed-size callback array is full.
The other two callback-array mechanisms in the codebase (WiFi manager's
status callbacks, BLE host's GAP event callbacks) both return
`dom_models_error_t` (`DOMAIN_MODELS_ERROR_BAD_STATE` on overflow) and use
identical swap-with-last-element removal. `add_callback` is changed to
match: signature becomes
`dom_models_error_t add_callback(..., cb_ctx, cb_func)`, and its two call
sites (the new `log_forwarding` usecase from pattern #2 — since MQTT and
BLE both become consumers of that usecase rather than calling
`add_callback` directly anymore) check and log the return value.

## Sequencing

Ordered so later steps build on already-normalized shapes rather than
needing to be redone:

1. **Logger contract change** (pattern #8) — `add_callback` returns
   `dom_models_error_t`. Touches the contract header, the stdio
   implementation, and (temporarily) the 2 existing direct call sites
   (`ble/handler/log/handler.c`, `application/internal/messaging_callbacks/impl.c`)
   until step 3 replaces them.
2. **Lifecycle shape normalization** (pattern #1) across
   `application/internal/{settings,ota,wifi_manager,messaging_callbacks}`
   and `presentation/mqtt/context` — mechanical split of existing `_new`
   logic into `_new`+`_init`/`_deinit`, no behavior change, but this must
   land before step 3 since the new `log_forwarding` usecase is built
   using this shape from the start, and `messaging_callbacks` needs its
   own `_init`/`_deinit` split before it can cleanly delegate to another
   usecase inside `_init` instead of `_new`.
3. **Extract `log_forwarding` usecase** (pattern #2) — new
   `domain/usecases/internal/log_forwarding` module; rewire
   `messaging_callbacks` and `ble/handler/log` to consume it instead of
   calling `dom_contracts_logger_leveled_t` directly; update
   `composition/main/*.c` wiring.
4. **MQTT DTO extraction** (pattern #3) — restructure
   `presentation/mqtt/handler/{action,ota}.c` into
   `handler/{action,ota}/{handler,dto,types}.{c,h}` subdirectories with
   the cJSON logic moved into `dto.c`.
5. **MQTT buffer ownership fix** (pattern #4) — move `on_message.c`'s
   stack-local topic buffers into `pres_mqtt_context_t`.
6. **Strip redundant error logs** (pattern #7) — remove the duplicate
   `logger->error` calls in the now-restructured `ota.c`/`action.c`
   handlers.
7. **`validate_cfg()` extraction** (pattern #6) — add the helper to the 4
   BLE modules + `mqtt/context.c`.
8. **Tag convention normalization** (pattern #5) — mechanical rename pass
   across ~10 MQTT/composition files + the `TAG_PATH`→`BASE_TAG` rename in
   `ble/host.c`. Done last since it's the highest file-count, lowest-risk
   change, and easiest to verify with a simple grep-diff review.

## Verification

Given there's no existing unit-test suite for this firmware (verification
today is `go build`-equivalent — `idf.py build` — plus the hardware-in-loop
scenario docs under `docs/agent_test/`), verification per step is:

- `idf.py build` clean after every step (no warnings introduced).
- After step 3 (log_forwarding usecase): re-run the BLE log-notification
  path from `scenario/10-ble-gatt-services.md` manually against real
  hardware to confirm log forwarding still reaches a subscribed BLE
  central, and re-run the MQTT log-over-broker check from
  `scenario/04-logging.md` (`LOG-01`/`LOG-02`) to confirm MQTT log
  forwarding is unaffected.
- After step 5 (MQTT buffer fix): re-run `scenario/03-mqtt-registration-status.md`'s
  `REG-01`–`REG-04` and `scenario/05-action-dispatch.md`'s `ACT-01`–`ACT-08`
  and `scenario/06-ota-update.md`'s `OTA-01`–`OTA-06` against the real
  device, since these are exactly the message-parsing paths this step
  touches.
- After step 8 (tag rename): grep for any remaining flat `TAG` definitions
  under `presentation/mqtt` and `composition` to confirm none were missed.
- Final full pass: re-run the complete `docs/agent_test/v1.0.0-dev.1/checklist.md`
  (all 42 original cases + the 11 BLE cases from scenario 10) to confirm
  zero regressions from a refactor that was explicitly scoped as
  "no behavior change."

## Explicitly out of scope

- Any change to wire payload shapes (MQTT topic strings/JSON fields, BLE
  GATT UUIDs/characteristic semantics) — backend and any existing BLE
  central integration must see zero difference.
- New features or new transports.
- Domain-layer data model changes.
- Audit categories #5 (DTO placement having "no codebase rule") and #10
  (no hard layer violations found) — these are informational findings,
  not action items; #5 is resolved as a side effect of pattern #3 above,
  #10 confirms no fix is needed.
