#ifndef INFRASTRUCTURE_SYSTEM_UPDATE_STUB_IMPL_UTILS_H
#define INFRASTRUCTURE_SYSTEM_UPDATE_STUB_IMPL_UTILS_H

#include "domain/models/error.h"
#include "domain/models/update.h"
#include "infrastructure/system/update/stub_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_system_update_stub_impl_load_cfg(
    inf_system_update_stub_impl_ctx_t*       ctx,
    const inf_system_update_stub_impl_cfg_t* cfg
);

dom_models_error_t inf_system_update_stub_impl_set_update(
    inf_system_update_stub_impl_ctx_t* ctx,
    const dom_models_update_info_t*    update_info
);

dom_models_error_t inf_system_update_stub_impl_validate_update_info(
    const dom_models_update_info_t* update_info
);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_UPDATE_STUB_IMPL_UTILS_H */
