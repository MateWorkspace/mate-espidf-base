#ifndef INFRASTRUCTURE_MESSAGING_DEF_PUB_STUB_IMPL_UTILS_H
#define INFRASTRUCTURE_MESSAGING_DEF_PUB_STUB_IMPL_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#include "domain/models/device_status.h"
#include "domain/models/error.h"
#include "infrastructure/messaging/def_pub/stub_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_messaging_def_pub_stub_impl_load_cfg(
    inf_messaging_def_pub_stub_impl_ctx_t*       ctx,
    const inf_messaging_def_pub_stub_impl_cfg_t* cfg
);

dom_models_error_t inf_messaging_def_pub_stub_impl_set_registration(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const char*                            device_info,
    const char*                            firmware_name
);

dom_models_error_t inf_messaging_def_pub_stub_impl_set_status(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    dom_models_device_status_t             status
);

dom_models_error_t inf_messaging_def_pub_stub_impl_set_log(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const char*                            msg,
    size_t                                 msg_len
);

dom_models_error_t inf_messaging_def_pub_stub_impl_set_action_ack(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const char*                            execution_id,
    const char*                            status,
    const char*                            message
);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_MESSAGING_DEF_PUB_STUB_IMPL_UTILS_H */
