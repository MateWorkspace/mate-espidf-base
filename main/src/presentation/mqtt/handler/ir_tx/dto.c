#include "presentation/mqtt/handler/ir_tx/dto.h"

#include <string.h>

#include "cJSON.h"

dom_models_error_t pres_mqtt_handler_ir_tx_dto_decode(
    const char*                            data,
    int                                    data_len,
    pres_mqtt_handler_ir_tx_dto_request_t* out
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
    cJSON* raw_data_item     = cJSON_GetObjectItemCaseSensitive(json, "raw_data");

    if (cJSON_IsString(execution_id_item) && execution_id_item->valuestring) {
        strncpy(out->execution_id, execution_id_item->valuestring, sizeof(out->execution_id) - 1);
        out->execution_id_set = true;
    }

    if (cJSON_IsArray(raw_data_item)) {
        int    array_size = cJSON_GetArraySize(raw_data_item);
        size_t count      = 0;

        if (array_size > PRES_MQTT_HANDLER_IR_TX_DTO_RAW_DATA_MAX_LEN) {
            out->raw_data_lossy = true;
        }

        for (int i = 0; i < array_size && count < PRES_MQTT_HANDLER_IR_TX_DTO_RAW_DATA_MAX_LEN; i++) {
            cJSON* item = cJSON_GetArrayItem(raw_data_item, i);
            if (!cJSON_IsNumber(item)) {
                out->raw_data_lossy = true;
                continue;
            }
            out->raw_data[count] = (int32_t)item->valuedouble;
            count++;
        }
        out->raw_data_count = count;
    }

    cJSON_Delete(json);

    return DOMAIN_MODELS_ERROR_OK;
}
