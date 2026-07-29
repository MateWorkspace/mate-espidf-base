#include "application/internal/system_info/impl_utils.h"

static bool has_system_info_functions(dom_contracts_system_info_t* system_info);

dom_models_error_t app_internal_system_info_impl_validate_cfg(const app_internal_system_info_impl_cfg_t* cfg) {
    if (!cfg ||
        !cfg->logger ||
        !cfg->logger->error ||
        !cfg->logger->info ||
        !has_system_info_functions(cfg->system_info)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static bool has_system_info_functions(dom_contracts_system_info_t* system_info) {
    return system_info &&
           system_info->get_project_info &&
           system_info->get_chip_info;
}
