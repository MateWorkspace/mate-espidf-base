#include "presentation/mqtt/event/on_connect.h"

#include "domain/models/error.h"

#define TAG "pres_mqtt_on_connect"

void pres_mqtt_event_on_connect(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    (void)event;

    ctx->logger->info(ctx->logger, TAG, "Connected to MQTT broker");

    dom_models_error_t err = ctx->messaging_callbacks->publish_registration(ctx->messaging_callbacks);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, TAG, "Failed to publish registration: %s (%d)", dom_models_error_str(err), (int)err);
    }

    err = ctx->messaging_callbacks->publish_online_status(ctx->messaging_callbacks);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, TAG, "Failed to publish online status: %s (%d)", dom_models_error_str(err), (int)err);
    }

    err = ctx->messaging_callbacks->subscribe_defaults(ctx->messaging_callbacks);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, TAG, "Failed to subscribe to default topics: %s (%d)", dom_models_error_str(err), (int)err);
    }
}
