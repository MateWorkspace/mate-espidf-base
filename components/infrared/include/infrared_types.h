#ifndef INFRARED_TYPES_H
#define INFRARED_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "soc/gpio_num.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INFRARED_RX_IDLE_TIMEOUT_US         20000
#define INFRARED_RX_MAX_DURATIONS           512
#define INFRARED_RX_QUEUE_DEPTH_DEFAULT     4
#define INFRARED_RX_QUEUE_DEPTH             INFRARED_RX_QUEUE_DEPTH_DEFAULT
#define INFRARED_RX_TASK_STACK_SIZE_DEFAULT 4096
#define INFRARED_RX_TASK_PRIORITY_DEFAULT   5
#define INFRARED_RX_TASK_POLL_MS_DEFAULT    100
#define INFRARED_TX_CARRIER_HZ              38000
#define INFRARED_TX_RESOLUTION_HZ           1000000
#define INFRARED_TX_MAX_RMT_DURATION_US     32767

typedef struct infrared_handle_s infrared_handle_t;

typedef struct {
    bool     level;
    uint32_t duration_us;
} infrared_duration_t;

typedef struct {
    const infrared_duration_t* durations;
    size_t                     duration_count;
    bool                       overflowed;
} infrared_rx_frame_t;

typedef void (*infrared_receive_cb_t)(const infrared_rx_frame_t* frame, void* user_ctx);

typedef struct {
    gpio_num_t            rx_gpio;
    gpio_num_t            tx_gpio;
    bool                  enable_rx;
    bool                  enable_tx;
    infrared_receive_cb_t on_receive;
    void*                 user_ctx;
    uint32_t              rx_task_stack_size;
    uint32_t              rx_task_priority;
    uint32_t              rx_task_poll_ms;
    size_t                rx_queue_depth;
} infrared_cfg_t;

#define INFRARED_CFG_DEFAULT()                                     \
    ((infrared_cfg_t){                                             \
        .rx_gpio            = GPIO_NUM_NC,                         \
        .tx_gpio            = GPIO_NUM_NC,                         \
        .enable_rx          = false,                               \
        .enable_tx          = false,                               \
        .on_receive         = NULL,                                \
        .user_ctx           = NULL,                                \
        .rx_task_stack_size = INFRARED_RX_TASK_STACK_SIZE_DEFAULT, \
        .rx_task_priority   = INFRARED_RX_TASK_PRIORITY_DEFAULT,   \
        .rx_task_poll_ms    = INFRARED_RX_TASK_POLL_MS_DEFAULT,    \
        .rx_queue_depth     = INFRARED_RX_QUEUE_DEPTH_DEFAULT,     \
    })

#ifdef __cplusplus
}
#endif

#endif /* INFRARED_TYPES_H */
