#include "presentation/ble/host_utils.h"

dom_models_error_t pres_ble_host_validate_cfg(const pres_ble_host_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->gatt_registry) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
