#include "presentation/mqtt/handler/action.h"

#include <stdint.h>
#include <string.h>

#include "cJSON.h"
#include "domain/models/error.h"

#define TAG "pres_mqtt_action"

#define ACTION_RESTART "restart"

void pres_mqtt_handler_action(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    ctx->logger->info(ctx->logger, TAG, "Received action request via MQTT");

    if (!data || data_len <= 0) {
        ctx->logger->error(ctx->logger, TAG, "Empty payload received");
        return;
    }

    cJSON* json = cJSON_ParseWithLength(data, (size_t)data_len);
    if (!json) {
        ctx->logger->error(ctx->logger, TAG, "Failed to parse JSON payload");
        return;
    }

    cJSON* action_item   = cJSON_GetObjectItemCaseSensitive(json, "action");
    cJSON* delay_ms_item = cJSON_GetObjectItemCaseSensitive(json, "delay_ms");

    if (!cJSON_IsString(action_item)) {
        ctx->logger->error(ctx->logger, TAG, "Invalid action payload: missing action field");
        cJSON_Delete(json);
        return;
    }

    uint32_t delay_ms = cJSON_IsNumber(delay_ms_item) ? (uint32_t)delay_ms_item->valuedouble : 0;

    if (strcmp(action_item->valuestring, ACTION_RESTART) == 0) {
        dom_models_error_t err = ctx->messaging_callbacks->restart(ctx->messaging_callbacks, delay_ms);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            ctx->logger->error(ctx->logger, TAG, "Failed to execute restart action: %s (%d)", dom_models_error_str(err), (int)err);
        } else {
            ctx->logger->info(ctx->logger, TAG, "Restart action requested successfully");
        }
    } else {
        ctx->logger->warn(ctx->logger, TAG, "Unknown action: %s", action_item->valuestring);
    }

    cJSON_Delete(json);
}
