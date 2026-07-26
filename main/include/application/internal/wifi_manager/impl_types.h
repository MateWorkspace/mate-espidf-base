#ifndef APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_TYPES_H
#define APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_TYPES_H

#include <stddef.h>

#include "domain/contracts/device/wifi.h"
#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/repository/wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_INTERNAL_WIFI_MANAGER_IMPL_DEFAULT_RECONNECT_MAX_TRIALS 5

typedef struct {
    dom_contracts_logger_leveled_t*       logger;
    dom_contracts_device_wifi_t*          wifi;
    dom_contracts_repository_wifi_t*      wifi_repository;
    dom_contracts_repository_preloaded_t* preloaded_repository;
    size_t                                reconnect_max_trials;
} app_internal_wifi_manager_impl_cfg_t;

typedef struct {
    app_internal_wifi_manager_impl_cfg_t cfg;
    size_t                               reconnect_trial_count;
} app_internal_wifi_manager_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_TYPES_H */
