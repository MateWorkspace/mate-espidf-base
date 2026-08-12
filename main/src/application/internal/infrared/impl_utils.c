#include "application/internal/infrared/impl_utils.h"

dom_models_error_t app_internal_infrared_impl_validate_cfg(const app_internal_infrared_impl_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->ir || !cfg->def_pub || !cfg->def_sub || !cfg->preloaded_repository) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_infrared_impl_raw_data_to_durations(
    const int32_t*            raw_data,
    size_t                    raw_data_count,
    dom_models_ir_duration_t* out
) {
    if (!raw_data || raw_data_count == 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    for (size_t i = 0; i < raw_data_count; i++) {
        if (raw_data[i] < 0) {
            return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        }
        out[i].level       = (i % 2) == 0; /* index 0 is always a mark */
        out[i].duration_us = (uint32_t)raw_data[i];
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_infrared_impl_durations_to_raw_data(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    int32_t*                        out
) {
    if (!durations || duration_count == 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    for (size_t i = 0; i < duration_count; i++) {
        out[i] = (int32_t)durations[i].duration_us;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
