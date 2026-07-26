#ifndef APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_UTILS_H
#define APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_UTILS_H

#include "application/internal/wifi_manager/impl_types.h"
#include "domain/models/error.h"
#include "domain/models/wifi.h"
#include "domain/usecases/internal/wifi_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_wifi_manager_impl_normalize_cfg(app_internal_wifi_manager_impl_cfg_t* cfg);

dom_models_error_t app_internal_wifi_manager_impl_validate_cfg(const app_internal_wifi_manager_impl_cfg_t* cfg);

dom_models_error_t app_internal_wifi_manager_impl_validate_credential(const dom_models_wifi_sta_connect_config_t* credential);

dom_models_error_t app_internal_wifi_manager_impl_load_stored_sta(
    app_internal_wifi_manager_impl_ctx_t*            ctx,
    dom_usecases_internal_wifi_manager_stored_sta_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_UTILS_H */
