#ifndef APPLICATION_INTERNAL_OTA_IMPL_H
#define APPLICATION_INTERNAL_OTA_IMPL_H

#include "application/internal/ota/impl_types.h"
#include "domain/usecases/internal/ota.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_ota_t* app_internal_ota_impl_new(const app_internal_ota_impl_cfg_t* cfg);

void app_internal_ota_impl_delete(dom_usecases_internal_ota_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_OTA_IMPL_H */
