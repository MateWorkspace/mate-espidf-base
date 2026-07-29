#include "presentation/mqtt/event/on_connect.h"

#define BASE_TAG "pres_mqtt_on_connect"

void pres_mqtt_event_on_connect(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    const char* tag = BASE_TAG "/handle";

    (void)event;

    ctx->logger->info(ctx->logger, tag, "Connected to MQTT broker");

    /* No error logs on failure below - each usecase call already logs its
       own failure (see app_internal_messaging_callbacks_impl.c); re-logging
       here would double-log the same condition. */
    (void)ctx->messaging_callbacks->publish_registration(ctx->messaging_callbacks);
    (void)ctx->messaging_callbacks->publish_online_status(ctx->messaging_callbacks);
    (void)ctx->messaging_callbacks->subscribe_defaults(ctx->messaging_callbacks);
}
