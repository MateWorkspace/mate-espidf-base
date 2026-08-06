#include "presentation/mqtt/handler/registration_ack/dto.h"

#include <string.h>

#include "cJSON.h"

dom_models_error_t pres_mqtt_handler_registration_ack_dto_decode(
    const char*                                        data,
    int                                                 data_len,
    pres_mqtt_handler_registration_ack_dto_request_t* out
) {
    if (!data || data_len <= 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));

    cJSON* json = cJSON_ParseWithLength(data, (size_t)data_len);
    if (!json) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* success_item = cJSON_GetObjectItemCaseSensitive(json, "success");
    if (cJSON_IsBool(success_item)) {
        out->success     = cJSON_IsTrue(success_item);
        out->success_set = true;
    }

    cJSON_Delete(json);

    return DOMAIN_MODELS_ERROR_OK;
}
