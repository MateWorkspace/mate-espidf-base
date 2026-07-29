#include "presentation/mqtt/event/on_error.h"

#define BASE_TAG "pres_mqtt_on_error"

void pres_mqtt_event_on_error(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->error(ctx->logger, tag, "MQTT error event type: %d", (int)event->error_handle->error_type);
}
