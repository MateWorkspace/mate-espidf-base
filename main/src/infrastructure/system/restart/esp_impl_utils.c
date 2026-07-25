#include "infrastructure/system/restart/esp_impl_utils.h"

#include "domain/models/error.h"

dom_models_error_t inf_system_restart_esp_impl_validate_cfg(
    const inf_system_restart_esp_impl_cfg_t* cfg
) {
    if (!cfg) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
