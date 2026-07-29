#include "presentation/mqtt/event/on_message.h"

#include <stdio.h>
#include <string.h>

#include "presentation/mqtt/handler/action/handler.h"
#include "presentation/mqtt/handler/ota/handler.h"
#include "presentation/mqtt/handler/registration_ack.h"

#define TAG "pres_mqtt_on_message"

void pres_mqtt_event_on_message(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event) {
    if (event->topic_len <= 0 || event->data_len < 0) {
        return;
    }

    char topic[128];
    if ((size_t)event->topic_len >= sizeof(topic)) {
        ctx->logger->error(ctx->logger, TAG, "Topic too long");
        return;
    }
    memcpy(topic, event->topic, (size_t)event->topic_len);
    topic[event->topic_len] = '\0';

    char registration_ack_expected[64];
    char ota_expected[64];
    char action_expected[64];
    snprintf(registration_ack_expected, sizeof(registration_ack_expected), "/sub/%s/registration_ack", ctx->device_id_str);
    snprintf(ota_expected, sizeof(ota_expected), "/sub/%s/ota", ctx->device_id_str);
    snprintf(action_expected, sizeof(action_expected), "/sub/%s/action", ctx->device_id_str);

    if (strcmp(topic, registration_ack_expected) == 0) {
        pres_mqtt_handler_registration_ack(ctx, event->data, event->data_len);
    } else if (strcmp(topic, ota_expected) == 0) {
        pres_mqtt_handler_ota(ctx, event->data, event->data_len);
    } else if (strcmp(topic, action_expected) == 0) {
        pres_mqtt_handler_action(ctx, event->data, event->data_len);
    } else {
        ctx->logger->debug(ctx->logger, TAG, "Unhandled topic: %s", topic);
    }
}
