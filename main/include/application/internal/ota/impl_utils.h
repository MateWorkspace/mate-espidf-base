#ifndef APPLICATION_INTERNAL_OTA_IMPL_UTILS_H
#define APPLICATION_INTERNAL_OTA_IMPL_UTILS_H

#include "application/internal/ota/impl_types.h"
#include "domain/models/error.h"
#include "domain/models/update.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_ota_impl_validate_cfg(const app_internal_ota_impl_cfg_t* cfg);

dom_models_error_t app_internal_ota_impl_validate_update_info(const dom_models_update_info_t* update_info);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_OTA_IMPL_UTILS_H */
