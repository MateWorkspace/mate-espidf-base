#include "presentation/ble/handler/system_info/utils.h"

dom_models_error_t pres_ble_handler_system_info_validate_cfg(const pres_ble_handler_system_info_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->system_info || !cfg->gatt_registry) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
