#ifndef DOMAIN_MODELS_PRELOADED_H
#define DOMAIN_MODELS_PRELOADED_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DOMAIN_MODELS_PRELOADED_MQTT_PROTO_KEY                   "mqtt_proto"
#define DOMAIN_MODELS_PRELOADED_MQTT_HOST_KEY                    "mqtt_host"
#define DOMAIN_MODELS_PRELOADED_MQTT_PORT_KEY                    "mqtt_port"
#define DOMAIN_MODELS_PRELOADED_MQTT_USER_KEY                    "mqtt_user"
#define DOMAIN_MODELS_PRELOADED_MQTT_PASS_KEY                    "mqtt_pass"
#define DOMAIN_MODELS_PRELOADED_SYSTEM_RESTART_AFTER_MS_KEY      "sys_rst_aft_ms"
#define DOMAIN_MODELS_PRELOADED_WIFI_STA_TRY_CONNECT_ON_INIT_KEY "wifi_try_init"

typedef struct {
    uint64_t device_id;
    char*    device_id_str;
    char*    mqtt_proto;
    char*    mqtt_host;
    char*    mqtt_port;
    char*    mqtt_user;
    char*    mqtt_pass;
    uint32_t system_restart_after_ms;
    bool     wifi_sta_try_connect_on_init;
} dom_models_preloaded_t;

extern dom_models_preloaded_t dom_models_preloaded_data;

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_PRELOADED_H */