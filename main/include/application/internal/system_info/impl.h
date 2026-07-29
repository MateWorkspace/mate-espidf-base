#ifndef APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_H
#define APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_H

#include "application/internal/system_info/impl_types.h"
#include "domain/usecases/internal/system_info.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_system_info_t* app_internal_system_info_impl_new(const app_internal_system_info_impl_cfg_t* cfg);

void app_internal_system_info_impl_delete(dom_usecases_internal_system_info_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_H */
