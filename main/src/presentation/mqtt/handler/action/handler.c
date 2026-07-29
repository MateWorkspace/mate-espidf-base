#include "presentation/mqtt/handler/action/handler.h"

#include <string.h>

#include "domain/models/error.h"
#include "presentation/mqtt/handler/action/dto.h"

#define BASE_TAG "pres_mqtt_action"

#define ACTION_RESTART "restart"

#define ACTION_ACK_STATUS_SUCCESS "SUCCESS"
#define ACTION_ACK_STATUS_FAILED  "FAILED"

void pres_mqtt_handler_action(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->info(ctx->logger, tag, "Received action request via MQTT");

    pres_mqtt_handler_action_dto_request_t request;
    dom_models_error_t                     err = pres_mqtt_handler_action_dto_decode(data, data_len, &request);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, tag, "Failed to parse action payload: %s (%d)", dom_models_error_str(err), (int)err);
        return;
    }

    if (!request.execution_id_set) {
        ctx->logger->error(ctx->logger, tag, "Invalid action payload: missing execution_id field");
        return;
    }

    if (!request.action_set) {
        ctx->logger->error(ctx->logger, tag, "Invalid action payload: missing action field");
        (void)ctx->messaging_callbacks->publish_action_ack(ctx->messaging_callbacks, request.execution_id, ACTION_ACK_STATUS_FAILED, "missing action field");
        return;
    }

    if (strcmp(request.action, ACTION_RESTART) == 0) {
        (void)ctx->messaging_callbacks->publish_action_ack(ctx->messaging_callbacks, request.execution_id, ACTION_ACK_STATUS_SUCCESS, NULL);

        err = ctx->messaging_callbacks->restart(ctx->messaging_callbacks, request.delay_ms);
        if (err == DOMAIN_MODELS_ERROR_OK) {
            ctx->logger->info(ctx->logger, tag, "Restart action requested successfully");
        }
        /* No error log on failure here - the usecase (messaging_callbacks'
           restart_impl) already logs the failure; re-logging the same
           condition here would double-log it (see Phase 6). */
    } else {
        ctx->logger->warn(ctx->logger, tag, "Unknown action: %s", request.action);
        (void)ctx->messaging_callbacks->publish_action_ack(ctx->messaging_callbacks, request.execution_id, ACTION_ACK_STATUS_FAILED, "unknown action");
    }
}
