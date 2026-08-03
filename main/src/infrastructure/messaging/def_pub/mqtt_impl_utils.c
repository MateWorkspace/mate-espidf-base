#include "infrastructure/messaging/def_pub/mqtt_impl_utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "mqtt_client.h"

/* Helper Function Prototypes */

static bool cstr_available(const char* value);

dom_models_error_t inf_messaging_def_pub_mqtt_impl_validate_cfg(
    const inf_messaging_def_pub_mqtt_impl_cfg_t* cfg
) {
    if (!cfg || !cfg->mqtt_client) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_pub_mqtt_impl_build_device_topic(
    const char* device_id,
    const char* suffix,
    char*       out,
    size_t      out_size
) {
    if (!cstr_available(device_id) || !cstr_available(suffix) || !out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int written = snprintf(out, out_size, "/pub/%s/%s", device_id, suffix);
    if (written <= 0 || (size_t)written >= out_size) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

char* inf_messaging_def_pub_mqtt_impl_build_registration_json(
    const char*                      device_id,
    const char*                      device_info,
    const char*                      firmware_name,
    const dom_models_preloaded_kv_t* config,
    size_t                           config_count
) {
    if (!cstr_available(device_id) || !cstr_available(device_info) || !cstr_available(firmware_name)) {
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "device_id", device_id) ||
        !cJSON_AddStringToObject(root, "device_info", device_info) ||
        !cJSON_AddStringToObject(root, "firmware_name", firmware_name)) {
        cJSON_Delete(root);
        return NULL;
    }

    /* Always emit a (possibly empty) "config" object - every value is a
       string, matching how node_config_values.value and the config-set
       MQTT payload already represent config values on the backend, so the
       registration handler there doesn't need a second parsing convention. */
    cJSON* config_obj = cJSON_AddObjectToObject(root, "config");
    if (!config_obj) {
        cJSON_Delete(root);
        return NULL;
    }
    for (size_t i = 0; i < config_count; i++) {
        if (!config || !config[i].key || !config[i].value) {
            continue;
        }
        if (!cJSON_AddStringToObject(config_obj, config[i].key, config[i].value)) {
            cJSON_Delete(root);
            return NULL;
        }
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

char* inf_messaging_def_pub_mqtt_impl_build_status_json(
    dom_models_device_status_t status
) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "status", dom_models_device_status_str(status))) {
        cJSON_Delete(root);
        return NULL;
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

char* inf_messaging_def_pub_mqtt_impl_build_action_ack_json(
    const char* execution_id,
    const char* status,
    const char* message
) {
    if (!cstr_available(execution_id) || !cstr_available(status)) {
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "execution_id", execution_id) ||
        !cJSON_AddStringToObject(root, "status", status)) {
        cJSON_Delete(root);
        return NULL;
    }

    if (cstr_available(message) && !cJSON_AddStringToObject(root, "message", message)) {
        cJSON_Delete(root);
        return NULL;
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

char* inf_messaging_def_pub_mqtt_impl_build_telemetry_json(
    const char* metric_name,
    const char* payload_schema_name,
    int         payload_schema_version,
    const char* payload_json
) {
    if (!cstr_available(metric_name) || !cstr_available(payload_schema_name) || !cstr_available(payload_json)) {
        return NULL;
    }

    cJSON* payload = cJSON_Parse(payload_json);
    if (!payload) {
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(payload);
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "metric_name", metric_name) ||
        !cJSON_AddStringToObject(root, "payload_schema_name", payload_schema_name) ||
        !cJSON_AddNumberToObject(root, "payload_schema_version", payload_schema_version) ||
        !cJSON_AddItemToObject(root, "payload", payload)) {
        cJSON_Delete(root);
        cJSON_Delete(payload);
        return NULL;
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

dom_models_error_t inf_messaging_def_pub_mqtt_impl_publish_json(
    const inf_messaging_def_pub_mqtt_impl_ctx_t* ctx,
    const char*                                  topic,
    char*                                        json,
    int                                          qos,
    bool                                          retain
) {
    if (!ctx || !ctx->cfg.mqtt_client || !cstr_available(topic)) {
        if (json) {
            cJSON_free(json);
        }
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (!json) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    size_t len = strlen(json);
    if (len == 0) {
        cJSON_free(json);
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int msg_id = esp_mqtt_client_publish(ctx->cfg.mqtt_client, topic, json, (int)len, qos, retain ? 1 : 0);
    cJSON_free(json);
    if (msg_id < 0) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_pub_mqtt_impl_publish_raw(
    const inf_messaging_def_pub_mqtt_impl_ctx_t* ctx,
    const char*                                  topic,
    const char*                                  data,
    size_t                                       data_len,
    int                                          qos
) {
    if (!ctx || !ctx->cfg.mqtt_client || !cstr_available(topic) || !data || data_len == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int msg_id = esp_mqtt_client_publish(ctx->cfg.mqtt_client, topic, data, (int)data_len, qos, 0);
    if (msg_id < 0) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static bool cstr_available(const char* value) {
    return value && value[0] != '\0';
}
