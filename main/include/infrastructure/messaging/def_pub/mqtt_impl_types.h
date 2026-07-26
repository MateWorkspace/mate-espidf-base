#ifndef INFRASTRUCTURE_MESSAGING_DEF_PUB_MQTT_IMPL_TYPES_H
#define INFRASTRUCTURE_MESSAGING_DEF_PUB_MQTT_IMPL_TYPES_H

#include <stdbool.h>

#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INF_MESSAGING_DEF_PUB_MQTT_IMPL_QOS_DEFAULT 1
#define INF_MESSAGING_DEF_PUB_MQTT_IMPL_QOS_LOG     0

typedef struct {
    esp_mqtt_client_handle_t mqtt_client;
} inf_messaging_def_pub_mqtt_impl_cfg_t;

#define INF_MESSAGING_DEF_PUB_MQTT_IMPL_CFG_DEFAULT() \
    {                                                 \
        .mqtt_client = NULL,                          \
    }

typedef struct {
    inf_messaging_def_pub_mqtt_impl_cfg_t cfg;
    bool                                  connected;
} inf_messaging_def_pub_mqtt_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_MESSAGING_DEF_PUB_MQTT_IMPL_TYPES_H */
