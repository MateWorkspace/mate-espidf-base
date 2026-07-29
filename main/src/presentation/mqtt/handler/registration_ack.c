#include "presentation/mqtt/handler/registration_ack.h"

#define BASE_TAG "pres_mqtt_registration_ack"

void pres_mqtt_handler_registration_ack(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    (void)data;
    (void)data_len;

    ctx->logger->info(ctx->logger, tag, "Received registration ack via MQTT");
}
