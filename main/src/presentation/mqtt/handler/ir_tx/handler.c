#include "presentation/mqtt/handler/ir_tx/handler.h"

#include "domain/models/error.h"
#include "presentation/mqtt/handler/ir_tx/dto.h"

#define BASE_TAG "pres_mqtt_ir_tx"

#define IR_TX_ACK_STATUS_FAILED "FAILED"

void pres_mqtt_handler_ir_tx(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->info(ctx->logger, tag, "Received ir/tx request via MQTT");

    pres_mqtt_handler_ir_tx_dto_request_t request;
    dom_models_error_t                    err = pres_mqtt_handler_ir_tx_dto_decode(data, data_len, &request);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, tag, "Failed to parse ir/tx payload: %s (%d)", dom_models_error_str(err), (int)err);
        return;
    }

    if (!request.execution_id_set) {
        ctx->logger->error(ctx->logger, tag, "Invalid ir/tx payload: missing execution_id field");
        return;
    }

    if (request.raw_data_count == 0) {
        ctx->logger->error(ctx->logger, tag, "Invalid ir/tx payload: missing or empty raw_data field");
        (void)ctx->def_pub->ir_transmit_ack(ctx->def_pub, ctx->device_id_str, request.execution_id, IR_TX_ACK_STATUS_FAILED, "missing raw_data field");
        return;
    }

    if (!ctx->infrared) {
        ctx->logger->warn(ctx->logger, tag, "ir/tx received but IR is not wired on this build");
        (void)ctx->def_pub->ir_transmit_ack(ctx->def_pub, ctx->device_id_str, request.execution_id, IR_TX_ACK_STATUS_FAILED, "IR not supported on this device");
        return;
    }

    (void)ctx->infrared->transmit(ctx->infrared, request.execution_id, request.raw_data, request.raw_data_count);
}
