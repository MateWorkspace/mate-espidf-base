#include "presentation/ble/handler/settings/utils.h"

dom_models_error_t pres_ble_handler_settings_validate_cfg(const pres_ble_handler_settings_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->settings || !cfg->gatt_registry) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
