#include "application/internal/ota/impl_utils.h"

#include <stdbool.h>

/* Helper Function Prototypes */

static bool cstr_available(const char* value);

dom_models_error_t app_internal_ota_impl_validate_cfg(const app_internal_ota_impl_cfg_t* cfg) {
    if (!cfg ||
        !cfg->logger ||
        !cfg->logger->error ||
        !cfg->logger->info ||
        !cfg->system_update ||
        !cfg->system_update->update ||
        !cfg->system_update->validate ||
        !cfg->system_update->rollback ||
        !cfg->system_update->add_event_callback ||
        !cfg->system_update->remove_event_callback ||
        !cfg->system_restart ||
        !cfg->system_restart->restart) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_ota_impl_validate_update_info(const dom_models_update_info_t* update_info) {
    if (!update_info ||
        !cstr_available(update_info->firmware_url) ||
        !cstr_available(update_info->firmware_checksum) ||
        update_info->firmware_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static bool cstr_available(const char* value) {
    return value && value[0] != '\0';
}
