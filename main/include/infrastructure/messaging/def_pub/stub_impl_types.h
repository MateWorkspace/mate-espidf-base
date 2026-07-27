#ifndef INFRASTRUCTURE_MESSAGING_DEF_PUB_STUB_IMPL_TYPES_H
#define INFRASTRUCTURE_MESSAGING_DEF_PUB_STUB_IMPL_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#include "domain/models/device_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INF_MESSAGING_DEF_PUB_STUB_IMPL_DEVICE_ID_MAX_LEN 37
#define INF_MESSAGING_DEF_PUB_STUB_IMPL_STR_MAX_LEN       96
#define INF_MESSAGING_DEF_PUB_STUB_IMPL_LOG_MAX_LEN       256

typedef struct {
    bool connected;
} inf_messaging_def_pub_stub_impl_cfg_t;

#define INF_MESSAGING_DEF_PUB_STUB_IMPL_CFG_DEFAULT() \
    {                                                 \
        .connected = true,                            \
    }

typedef struct {
    bool                       connected;
    char                       last_registration_device_id[INF_MESSAGING_DEF_PUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    char                       last_registration_device_info[INF_MESSAGING_DEF_PUB_STUB_IMPL_STR_MAX_LEN];
    char                       last_registration_firmware_name[INF_MESSAGING_DEF_PUB_STUB_IMPL_STR_MAX_LEN];
    char                       last_status_device_id[INF_MESSAGING_DEF_PUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    dom_models_device_status_t last_status;
    char                       last_log_device_id[INF_MESSAGING_DEF_PUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    char                       last_log_message[INF_MESSAGING_DEF_PUB_STUB_IMPL_LOG_MAX_LEN];
    char                       last_action_ack_device_id[INF_MESSAGING_DEF_PUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    char                       last_action_ack_execution_id[INF_MESSAGING_DEF_PUB_STUB_IMPL_STR_MAX_LEN];
    char                       last_action_ack_status[INF_MESSAGING_DEF_PUB_STUB_IMPL_STR_MAX_LEN];
    char                       last_action_ack_message[INF_MESSAGING_DEF_PUB_STUB_IMPL_STR_MAX_LEN];
    size_t                     registration_publish_cnt;
    size_t                     status_publish_cnt;
    size_t                     log_publish_cnt;
    size_t                     action_ack_publish_cnt;
} inf_messaging_def_pub_stub_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_MESSAGING_DEF_PUB_STUB_IMPL_TYPES_H */
