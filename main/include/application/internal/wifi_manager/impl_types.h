#ifndef APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_TYPES_H
#define APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_TYPES_H

#include <stdbool.h>

#include "domain/contracts/device/wifi.h"
#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/repository/wifi.h"
#include "domain/models/wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_STATUS_CB_MAX_CNT 2

typedef struct {
    dom_contracts_logger_leveled_t*       logger;
    dom_contracts_device_wifi_t*          wifi;
    dom_contracts_repository_wifi_t*      wifi_repository;
    dom_contracts_repository_preloaded_t* preloaded_repository;
} app_internal_wifi_manager_impl_cfg_t;

typedef struct {
    app_internal_wifi_manager_impl_cfg_t cfg;
    bool                                 connect_attempted;
    bool                                 event_subscribed;
    dom_models_wifi_event_callback_t     status_cb_funcs[APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_STATUS_CB_MAX_CNT];
    void*                                status_cb_ctxs[APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_STATUS_CB_MAX_CNT];
    unsigned int                         status_cb_idx;
} app_internal_wifi_manager_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_TYPES_H */
