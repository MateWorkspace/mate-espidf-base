#include "presentation/ble/handler/log/utils.h"

dom_models_error_t pres_ble_handler_log_validate_cfg(const pres_ble_handler_log_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->log_forwarding || !cfg->gatt_registry || !cfg->host) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
