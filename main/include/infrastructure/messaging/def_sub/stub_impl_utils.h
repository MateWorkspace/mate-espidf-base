#ifndef INFRASTRUCTURE_MESSAGING_DEF_SUB_STUB_IMPL_UTILS_H
#define INFRASTRUCTURE_MESSAGING_DEF_SUB_STUB_IMPL_UTILS_H

#include "domain/models/error.h"
#include "infrastructure/messaging/def_sub/stub_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_messaging_def_sub_stub_impl_load_cfg(
    inf_messaging_def_sub_stub_impl_ctx_t*       ctx,
    const inf_messaging_def_sub_stub_impl_cfg_t* cfg
);

dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_registration_ack(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
);

dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_ota(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
);

dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_action(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
);

dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_config(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_MESSAGING_DEF_SUB_STUB_IMPL_UTILS_H */
