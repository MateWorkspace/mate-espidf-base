#include "application/internal/log_forwarding/impl_utils.h"

dom_models_error_t app_internal_log_forwarding_impl_validate_cfg(const app_internal_log_forwarding_impl_cfg_t* cfg) {
    if (!cfg ||
        !cfg->logger ||
        !cfg->logger->error ||
        !cfg->logger->info ||
        !cfg->logger->add_callback ||
        !cfg->logger->remove_callback) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
