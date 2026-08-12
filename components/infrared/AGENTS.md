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
