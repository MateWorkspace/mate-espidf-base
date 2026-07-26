#include "presentation/mqtt/handler/ota.h"

#include <string.h>

#include "cJSON.h"
#include "domain/models/error.h"
#include "domain/models/update.h"

#define TAG "pres_mqtt_ota"

void pres_mqtt_handler_ota(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    ctx->logger->info(ctx->logger, TAG, "Received OTA update trigger via MQTT");

    if (!data || data_len <= 0) {
        ctx->logger->error(ctx->logger, TAG, "Empty payload received");
        return;
    }

    cJSON* json = cJSON_ParseWithLength(data, (size_t)data_len);
    if (!json) {
        ctx->logger->error(ctx->logger, TAG, "Failed to parse JSON payload");
        return;
    }

    cJSON* url_item      = cJSON_GetObjectItemCaseSensitive(json, "url");
    cJSON* size_item     = cJSON_GetObjectItemCaseSensitive(json, "size");
    cJSON* checksum_item = cJSON_GetObjectItemCaseSensitive(json, "checksum");

    if (!cJSON_IsString(url_item) || !cJSON_IsNumber(size_item) || !cJSON_IsString(checksum_item)) {
        ctx->logger->error(ctx->logger, TAG, "Invalid OTA payload fields");
        cJSON_Delete(json);
        return;
    }

    dom_models_update_info_t update_info;
    memset(&update_info, 0, sizeof(update_info));
    strncpy(update_info.firmware_url, url_item->valuestring, sizeof(update_info.firmware_url) - 1);
    update_info.firmware_size = (size_t)size_item->valuedouble;
    strncpy(update_info.firmware_checksum, checksum_item->valuestring, sizeof(update_info.firmware_checksum) - 1);

    cJSON_Delete(json);

    dom_models_error_t err = ctx->ota->update(ctx->ota, &update_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, TAG, "Failed to request OTA update: %s (%d)", dom_models_error_str(err), (int)err);
        return;
    }

    ctx->logger->info(ctx->logger, TAG, "OTA update requested for URL: %s", update_info.firmware_url);
}
