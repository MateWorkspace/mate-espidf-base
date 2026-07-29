#include "presentation/mqtt/handler/ota/dto.h"

#include <string.h>

#include "cJSON.h"

dom_models_error_t pres_mqtt_handler_ota_dto_decode(
    const char*                data,
    int                        data_len,
    dom_models_update_info_t* out
) {
    if (!data || data_len <= 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* json = cJSON_ParseWithLength(data, (size_t)data_len);
    if (!json) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* url_item      = cJSON_GetObjectItemCaseSensitive(json, "firmware_url");
    cJSON* size_item     = cJSON_GetObjectItemCaseSensitive(json, "firmware_size");
    cJSON* checksum_item = cJSON_GetObjectItemCaseSensitive(json, "firmware_checksum");

    if (!cJSON_IsString(url_item) || !cJSON_IsNumber(size_item) || !cJSON_IsString(checksum_item)) {
        cJSON_Delete(json);
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->firmware_url, url_item->valuestring, sizeof(out->firmware_url) - 1);
    out->firmware_size = (size_t)size_item->valuedouble;
    strncpy(out->firmware_checksum, checksum_item->valuestring, sizeof(out->firmware_checksum) - 1);

    cJSON_Delete(json);

    return DOMAIN_MODELS_ERROR_OK;
}
