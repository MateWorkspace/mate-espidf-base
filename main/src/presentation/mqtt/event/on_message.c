#include "presentation/mqtt/event/on_message.h"

#include <string.h>

#include "presentation/mqtt/handler/action/handler.h"
#include "presentation/mqtt/handler/config/handler.h"
#include "presentation/mqtt/handler/ir_tx/handler.h"
#include "presentation/mqtt/handler/ota/handler.h"
#include "presentation/mqtt/handler/registration_ack/handler.h"

#define BASE_TAG "pres_mqtt_on_message"

void pres_mqtt_event_on_message(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    const char* tag = BASE_TAG "/handle";

    if (event->topic_len <= 0 || event->data_len < 0) {
        return;
    }

    if ((size_t)event->topic_len >= sizeof(ctx->topic_scratch)) {
        ctx->logger->error(ctx->logger, tag, "Topic too long");
        return;
    }
    memcpy(ctx->topic_scratch, event->topic, (size_t)event->topic_len);
    ctx->topic_scratch[event->topic_len] = '\0';

    if (strcmp(ctx->topic_scratch, ctx->registration_ack_topic) == 0) {
        pres_mqtt_handler_registration_ack(ctx, event->data, event->data_len);
    } else if (strcmp(ctx->topic_scratch, ctx->ota_topic) == 0) {
        pres_mqtt_handler_ota(ctx, event->data, event->data_len);
    } else if (strcmp(ctx->topic_scratch, ctx->action_topic) == 0) {
        pres_mqtt_handler_action(ctx, event->data, event->data_len);
    } else if (strcmp(ctx->topic_scratch, ctx->config_topic) == 0) {
        pres_mqtt_handler_config(ctx, event->data, event->data_len);
    } else if (strcmp(ctx->topic_scratch, ctx->ir_tx_topic) == 0) {
        pres_mqtt_handler_ir_tx(ctx, event->data, event->data_len);
    } else {
        ctx->logger->debug(ctx->logger, tag, "Unhandled topic: %s", ctx->topic_scratch);
    }
}
