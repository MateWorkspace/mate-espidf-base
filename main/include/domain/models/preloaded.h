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

typedef enum {
    DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING,
    DOMAIN_MODELS_PRELOADED_VALUE_TYPE_UINT32,
    DOMAIN_MODELS_PRELOADED_VALUE_TYPE_BOOL,
} dom_models_preloaded_value_type_t;

#define DOMAIN_MODELS_PRELOADED_SCHEMA(X)                                                                                      \
    X(MQTT_PROTO, DOMAIN_MODELS_PRELOADED_MQTT_PROTO_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                           \
    X(MQTT_HOST, DOMAIN_MODELS_PRELOADED_MQTT_HOST_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                             \
    X(MQTT_PORT, DOMAIN_MODELS_PRELOADED_MQTT_PORT_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                             \
    X(MQTT_USER, DOMAIN_MODELS_PRELOADED_MQTT_USER_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                             \
    X(MQTT_PASS, DOMAIN_MODELS_PRELOADED_MQTT_PASS_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING)                             \
    X(SYSTEM_RESTART_AFTER_MS, DOMAIN_MODELS_PRELOADED_SYSTEM_RESTART_AFTER_MS_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_UINT32) \
    X(WIFI_STA_TRY_CONNECT_ON_INIT, DOMAIN_MODELS_PRELOADED_WIFI_STA_TRY_CONNECT_ON_INIT_KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_BOOL)

typedef struct {
    const char*                       key;
    dom_models_preloaded_value_type_t type;
} dom_models_preloaded_schema_entry_t;

#define DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT_X(cb_name, cb_key, cb_type) +1
#define DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT                             (0 DOMAIN_MODELS_PRELOADED_SCHEMA(DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT_X))

extern const dom_models_preloaded_schema_entry_t dom_models_preloaded_schema[DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT];

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