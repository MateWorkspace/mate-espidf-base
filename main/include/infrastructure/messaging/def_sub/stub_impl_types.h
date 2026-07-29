#ifndef INFRASTRUCTURE_MESSAGING_DEF_SUB_STUB_IMPL_TYPES_H
#define INFRASTRUCTURE_MESSAGING_DEF_SUB_STUB_IMPL_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INF_MESSAGING_DEF_SUB_STUB_IMPL_DEVICE_ID_MAX_LEN 37

typedef struct {
    bool registration_ack_subscribed;
    bool ota_subscribed;
    bool action_subscribed;
    bool config_subscribed;
} inf_messaging_def_sub_stub_impl_cfg_t;

#define INF_MESSAGING_DEF_SUB_STUB_IMPL_CFG_DEFAULT() \
    {                                                 \
        .registration_ack_subscribed = false,         \
        .ota_subscribed              = false,         \
        .action_subscribed           = false,         \
        .config_subscribed           = false,         \
    }

typedef struct {
    bool   registration_ack_subscribed;
    bool   ota_subscribed;
    bool   action_subscribed;
    bool   config_subscribed;
    char   last_registration_ack_device_id[INF_MESSAGING_DEF_SUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    char   last_ota_device_id[INF_MESSAGING_DEF_SUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    char   last_action_device_id[INF_MESSAGING_DEF_SUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    char   last_config_device_id[INF_MESSAGING_DEF_SUB_STUB_IMPL_DEVICE_ID_MAX_LEN];
    size_t registration_ack_subscribe_cnt;
    size_t ota_subscribe_cnt;
    size_t action_subscribe_cnt;
    size_t config_subscribe_cnt;
} inf_messaging_def_sub_stub_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_MESSAGING_DEF_SUB_STUB_IMPL_TYPES_H */
