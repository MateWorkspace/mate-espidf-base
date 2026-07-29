#include "presentation/mqtt/event/on_disconnect.h"

#define BASE_TAG "pres_mqtt_on_disconnect"

void pres_mqtt_event_on_disconnect(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    const char* tag = BASE_TAG "/handle";

    (void)event;

    ctx->logger->warn(ctx->logger, tag, "Disconnected from MQTT broker");
}
