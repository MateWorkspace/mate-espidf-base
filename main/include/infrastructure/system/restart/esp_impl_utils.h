#ifndef INFRASTRUCTURE_SYSTEM_RESTART_ESP_IMPL_UTILS_H
#define INFRASTRUCTURE_SYSTEM_RESTART_ESP_IMPL_UTILS_H

#include "domain/models/error.h"
#include "infrastructure/system/restart/esp_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_system_restart_esp_impl_validate_cfg(
    const inf_system_restart_esp_impl_cfg_t* cfg
);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_RESTART_ESP_IMPL_UTILS_H */
