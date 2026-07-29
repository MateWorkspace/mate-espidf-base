#include "presentation/mqtt/handler/action/dto.h"

#include <string.h>

#include "cJSON.h"

dom_models_error_t pres_mqtt_handler_action_dto_decode(
    const char*                             data,
    int                                     data_len,
    pres_mqtt_handler_action_dto_request_t* out
) {
    if (!data || data_len <= 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));

    cJSON* json = cJSON_ParseWithLength(data, (size_t)data_len);
    if (!json) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* execution_id_item = cJSON_GetObjectItemCaseSensitive(json, "execution_id");
    cJSON* action_item       = cJSON_GetObjectItemCaseSensitive(json, "action");
    cJSON* payload_item      = cJSON_GetObjectItemCaseSensitive(json, "payload");

    if (cJSON_IsString(execution_id_item) && execution_id_item->valuestring) {
        strncpy(out->execution_id, execution_id_item->valuestring, sizeof(out->execution_id) - 1);
        out->execution_id_set = true;
    }

    if (cJSON_IsString(action_item) && action_item->valuestring) {
        strncpy(out->action, action_item->valuestring, sizeof(out->action) - 1);
        out->action_set = true;
    }

    cJSON* delay_ms_item = cJSON_IsObject(payload_item) ? cJSON_GetObjectItemCaseSensitive(payload_item, "delay_ms") : NULL;
    out->delay_ms        = cJSON_IsNumber(delay_ms_item) ? (uint32_t)delay_ms_item->valuedouble : 0;

    cJSON_Delete(json);

    return DOMAIN_MODELS_ERROR_OK;
}
