#include "presentation/mqtt/handler/registration_ack/handler.h"

#include "domain/models/error.h"
#include "presentation/mqtt/handler/registration_ack/dto.h"

#define BASE_TAG "pres_mqtt_registration_ack"

void pres_mqtt_handler_registration_ack(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->info(ctx->logger, tag, "Received registration ack via MQTT");

    pres_mqtt_handler_registration_ack_dto_request_t request;
    dom_models_error_t                               err = pres_mqtt_handler_registration_ack_dto_decode(data, data_len, &request);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, tag, "Failed to parse registration ack payload: %s (%d)", dom_models_error_str(err), (int)err);
        ctx->messaging_callbacks->restart(ctx->messaging_callbacks, 0);
        return;
    }

    if (!request.success_set) {
        ctx->logger->error(ctx->logger, tag, "Invalid registration ack payload: missing success field");
        ctx->messaging_callbacks->restart(ctx->messaging_callbacks, 0);
        return;
    }

    if (!request.success) {
        ctx->logger->error(ctx->logger, tag, "Registration failed on the backend");
        ctx->messaging_callbacks->restart(ctx->messaging_callbacks, 0);
        return;
    }

    ctx->logger->info(ctx->logger, tag, "Registration acknowledged successfully");
}
