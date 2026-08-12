#ifndef INFRARED_INTERNAL_H
#define INFRARED_INTERNAL_H

#include "infrared_types.h"

#include "driver/rmt_types.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    infrared_duration_t durations[INFRARED_RX_MAX_DURATIONS];
    size_t              duration_count;
    bool                overflowed;
} infrared_rx_queue_item_t;

struct infrared_handle_s {
    infrared_cfg_t cfg;
    bool           initialized;

    bool              rx_initialized;
    bool              rx_gpio_configured;
    bool              rx_isr_added;
    QueueHandle_t     rx_queue;
    TimerHandle_t     rx_idle_timer;
    TaskHandle_t      rx_task;
    SemaphoreHandle_t rx_task_done;
    volatile bool     rx_task_stop;
    portMUX_TYPE      rx_mux;

    infrared_duration_t      rx_buffer[INFRARED_RX_MAX_DURATIONS];
    infrared_rx_queue_item_t rx_queue_item;
    infrared_rx_queue_item_t rx_dispatch_item;
    size_t                   rx_count;
    bool                     rx_active;
    bool                     rx_overflowed;
    int64_t                  rx_last_edge_us;
    bool                     rx_last_level;
    uint32_t                 rx_dropped_frames;
    uint32_t                 rx_overflowed_frames;

    bool                 tx_initialized;
    bool                 tx_enabled;
    SemaphoreHandle_t    tx_mutex;
    rmt_channel_handle_t tx_channel;
    rmt_encoder_handle_t tx_encoder;
};

#ifdef __cplusplus
}
#endif

#endif /* INFRARED_INTERNAL_H */
