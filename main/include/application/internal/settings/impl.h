#ifndef APPLICATION_INTERNAL_SETTINGS_IMPL_H
#define APPLICATION_INTERNAL_SETTINGS_IMPL_H

#include "application/internal/settings/impl_types.h"
#include "domain/usecases/internal/settings.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_settings_t* app_internal_settings_impl_new(const app_internal_settings_impl_cfg_t* cfg);

void app_internal_settings_impl_delete(dom_usecases_internal_settings_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_SETTINGS_IMPL_H */
