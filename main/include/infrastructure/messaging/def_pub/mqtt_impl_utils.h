#ifndef INFRASTRUCTURE_MESSAGING_DEF_PUB_MQTT_IMPL_UTILS_H
#define INFRASTRUCTURE_MESSAGING_DEF_PUB_MQTT_IMPL_UTILS_H

#include <stddef.h>

#include "domain/models/error.h"
#include "infrastructure/messaging/def_pub/mqtt_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INF_MESSAGING_DEF_PUB_MQTT_IMPL_TOPIC_MAX_LEN 128

dom_models_error_t inf_messaging_def_pub_mqtt_impl_validate_cfg(
    const inf_messaging_def_pub_mqtt_impl_cfg_t* cfg
);

dom_models_error_t inf_messaging_def_pub_mqtt_impl_build_device_topic(
    const char* device_id,
    const char* suffix,
    char*       out,
    size_t      out_size
);

char* inf_messaging_def_pub_mqtt_impl_build_registration_json(
    const char* device_id,
    const char* device_info,
    const char* firmware_name
);

char* inf_messaging_def_pub_mqtt_impl_build_status_json(
    const char* status
);

dom_models_error_t inf_messaging_def_pub_mqtt_impl_publish_json(
    const inf_messaging_def_pub_mqtt_impl_ctx_t* ctx,
    const char*                                  topic,
    char*                                        json,
    int                                          qos
);

dom_models_error_t inf_messaging_def_pub_mqtt_impl_publish_raw(
    const inf_messaging_def_pub_mqtt_impl_ctx_t* ctx,
    const char*                                  topic,
    const char*                                  data,
    size_t                                       data_len,
    int                                          qos
);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_MESSAGING_DEF_PUB_MQTT_IMPL_UTILS_H */
