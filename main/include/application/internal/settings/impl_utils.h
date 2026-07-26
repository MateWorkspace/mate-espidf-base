#ifndef APPLICATION_INTERNAL_SETTINGS_IMPL_UTILS_H
#define APPLICATION_INTERNAL_SETTINGS_IMPL_UTILS_H

#include <stdbool.h>

#include "application/internal/settings/impl_types.h"
#include "domain/models/error.h"
#include "domain/usecases/internal/settings.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_settings_impl_validate_cfg(const app_internal_settings_impl_cfg_t* cfg);

dom_models_error_t app_internal_settings_impl_load_snapshot(
    app_internal_settings_impl_ctx_t*          ctx,
    dom_usecases_internal_settings_snapshot_t* out
);

bool app_internal_settings_impl_has_preloaded_update(const dom_usecases_internal_settings_preloaded_update_t* update);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_SETTINGS_IMPL_UTILS_H */
