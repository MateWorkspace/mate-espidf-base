#include "presentation/mqtt/handler/config/dto.h"

#include <string.h>

#include "cJSON.h"

dom_models_error_t pres_mqtt_handler_config_dto_decode(
    const char*                              data,
    int                                       data_len,
    pres_mqtt_handler_config_dto_request_t* out
) {
    if (!data || data_len <= 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));

    cJSON* json = cJSON_ParseWithLength(data, (size_t)data_len);
    if (!json) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* key_item   = cJSON_GetObjectItemCaseSensitive(json, "key");
    cJSON* value_item = cJSON_GetObjectItemCaseSensitive(json, "value");

    if (cJSON_IsString(key_item) && key_item->valuestring) {
        strncpy(out->key, key_item->valuestring, sizeof(out->key) - 1);
        out->key_set = true;
    }

    if (cJSON_IsString(value_item) && value_item->valuestring) {
        strncpy(out->value, value_item->valuestring, sizeof(out->value) - 1);
        out->value_set = true;
    }

    cJSON_Delete(json);

    return DOMAIN_MODELS_ERROR_OK;
}
