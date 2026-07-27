#ifndef APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_H
#define APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_H

#include "application/internal/wifi_manager/impl_types.h"
#include "domain/usecases/internal/wifi_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_wifi_manager_t* app_internal_wifi_manager_impl_new(const app_internal_wifi_manager_impl_cfg_t* cfg);

void app_internal_wifi_manager_impl_delete(dom_usecases_internal_wifi_manager_t* self);

dom_models_error_t app_internal_wifi_manager_impl_init(dom_usecases_internal_wifi_manager_t* self);

void app_internal_wifi_manager_impl_deinit(dom_usecases_internal_wifi_manager_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_H */
