#ifndef APPLICATION_INTERNAL_MESSAGING_CALLBACKS_IMPL_UTILS_H
#define APPLICATION_INTERNAL_MESSAGING_CALLBACKS_IMPL_UTILS_H

#include <stddef.h>

#include "application/internal/messaging_callbacks/impl_types.h"
#include "domain/models/error.h"
#include "domain/models/system.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_messaging_callbacks_impl_validate_cfg(const app_internal_messaging_callbacks_impl_cfg_t* cfg);

dom_models_error_t app_internal_messaging_callbacks_impl_build_firmware_name(
    const dom_models_system_project_info_t* project_info,
    char*                                   out,
    size_t                                  out_size
);

dom_models_error_t app_internal_messaging_callbacks_impl_build_device_info(
    const dom_models_system_chip_info_t* chip_info,
    char*                                out,
    size_t                               out_size
);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_MESSAGING_CALLBACKS_IMPL_UTILS_H */
