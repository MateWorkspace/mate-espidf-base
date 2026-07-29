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
