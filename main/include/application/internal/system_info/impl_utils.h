#ifndef APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_UTILS_H
#define APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_UTILS_H

#include "application/internal/system_info/impl_types.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_system_info_impl_validate_cfg(const app_internal_system_info_impl_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_UTILS_H */
