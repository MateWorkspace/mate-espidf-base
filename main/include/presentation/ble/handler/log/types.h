#ifndef PRESENTATION_BLE_HANDLER_LOG_TYPES_H
#define PRESENTATION_BLE_HANDLER_LOG_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/usecases/internal/log_forwarding.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"
#include "presentation/ble/gatt/registry.h"
#include "presentation/ble/host.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sized well under the configured ATT MTU (256, see sdkconfig.defaults),
   smaller than idf-base's 244-byte precedent - this is the one-time,
   bounded stack cost added to whichever task calls logger->error/warn/
   info/debug(...) when BLE log forwarding is gated in (see handler.c's
   on_log_message). */
#define PRES_BLE_HANDLER_LOG_MESSAGE_MAX_LEN 180
#define PRES_BLE_HANDLER_LOG_QUEUE_LEN       16
#define PRES_BLE_HANDLER_LOG_TASK_STACK_SIZE 3072
#define PRES_BLE_HANDLER_LOG_TASK_PRIORITY   3
#define PRES_BLE_HANDLER_LOG_TASK_NAME       "ble_log"

typedef struct {
    char   message[PRES_BLE_HANDLER_LOG_MESSAGE_MAX_LEN];
    size_t message_len;
} pres_ble_handler_log_queue_item_t;

typedef struct {
    dom_contracts_logger_leveled_t*         logger;
    dom_usecases_internal_log_forwarding_t* log_forwarding;
    pres_ble_gatt_registry_t*               gatt_registry;
    pres_ble_host_t*                        host;
} pres_ble_handler_log_cfg_t;

typedef struct pres_ble_handler_log_t {
    pres_ble_handler_log_cfg_t cfg;
    bool                       registered;
    bool                       logger_cb_subscribed;
    bool                       gap_cb_subscribed;

    /* Read by on_log_message() on whatever task is logging - must stay
       cheap (plain volatile reads only, no locking) since that task is
       never one this module owns. */
    volatile bool enabled;
    volatile bool subscribed;

    uint16_t      message_chr_hdl;
    QueueHandle_t queue;
    TaskHandle_t  task_handle;

    /* Owned exclusively by the dedicated ble_log task - never touched by
       on_log_message() or by any caller's task. */
    char   message[PRES_BLE_HANDLER_LOG_MESSAGE_MAX_LEN];
    size_t message_len;
} pres_ble_handler_log_t;

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_LOG_TYPES_H */
