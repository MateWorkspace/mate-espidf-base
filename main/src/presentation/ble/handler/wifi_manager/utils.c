#include "presentation/ble/handler/wifi_manager/utils.h"

dom_models_error_t pres_ble_handler_wifi_manager_validate_cfg(const pres_ble_handler_wifi_manager_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->wifi_manager || !cfg->gatt_registry) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
