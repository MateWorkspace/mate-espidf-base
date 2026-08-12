#include "application/internal/infrared/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/infrared/impl_utils.h"
#include "domain/models/error.h"
#include "domain/models/ir.h"

#define BASE_TAG                "app_internal_infrared"
#define IR_TRANSMIT_ACK_SUCCESS "SUCCESS"
#define IR_TRANSMIT_ACK_FAILED  "FAILED"
#define IR_RAW_DATA_MAX_LEN     512 /* matches INFRARED_RX_MAX_DURATIONS */

/* Contract Function Prototypes */

static dom_models_error_t subscribe_impl(
    dom_usecases_internal_infrared_t* self
);
static dom_models_error_t transmit_impl(
    dom_usecases_internal_infrared_t* self,
    const char*                       execution_id,
    const int32_t*                    raw_data,
    size_t                            raw_data_count
);

/* Helper Function Prototypes */

static void on_ir_receive(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    void*                           cb_ctx
);

/* Constructor and Destructor */

dom_usecases_internal_infrared_t* app_internal_infrared_impl_new(const app_internal_infrared_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    if (app_internal_infrared_impl_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_infrared_impl_ctx_t* ctx = (app_internal_infrared_impl_ctx_t*)calloc(1, sizeof(app_internal_infrared_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_infrared_impl_cfg_t));

    dom_models_error_t err = ctx->cfg.preloaded_repository->get_device_id_str(ctx->cfg.preloaded_repository, ctx->device_id_str, sizeof(ctx->device_id_str));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load device id: %s (%d)", dom_models_error_str(err), (int)err);
        free(ctx);
        return NULL;
    }

    dom_usecases_internal_infrared_t* self = dom_usecases_internal_infrared_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->subscribe = subscribe_impl;
    self->transmit  = transmit_impl;

    return self;
}

void app_internal_infrared_impl_delete(dom_usecases_internal_infrared_t* self) {
    if (!self) {
        return;
    }

    app_internal_infrared_impl_deinit(self);

    free(self->ctx);
    dom_usecases_internal_infrared_delete(self);
}

/* Lifecycle */

dom_models_error_t app_internal_infrared_impl_init(dom_usecases_internal_infrared_t* self) {
    const char* tag = BASE_TAG "/init";

    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    app_internal_infrared_impl_ctx_t* ctx = self->ctx;
    if (ctx->receive_handler_registered) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    dom_models_error_t err = ctx->cfg.ir->set_receive_handler(ctx->cfg.ir, ctx, on_ir_receive);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to register IR receive handler: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->receive_handler_registered = true;

    return DOMAIN_MODELS_ERROR_OK;
}

void app_internal_infrared_impl_deinit(dom_usecases_internal_infrared_t* self) {
    if (!self || !self->ctx) {
        return;
    }

    app_internal_infrared_impl_ctx_t* ctx = self->ctx;
    if (!ctx->receive_handler_registered) {
        return;
    }

    (void)ctx->cfg.ir->set_receive_handler(ctx->cfg.ir, NULL, NULL);
    ctx->receive_handler_registered = false;
}

/* Contract Function Implementations */

static dom_models_error_t subscribe_impl(
    dom_usecases_internal_infrared_t* self
) {
    const char* tag = BASE_TAG "/subscribe";

    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    app_internal_infrared_impl_ctx_t* ctx = self->ctx;

    dom_models_error_t err = ctx->cfg.def_sub->ir_tx(ctx->device_id_str, ctx->cfg.def_sub);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to ir/tx: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Subscribed to ir/tx successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t transmit_impl(
    dom_usecases_internal_infrared_t* self,
    const char*                       execution_id,
    const int32_t*                    raw_data,
    size_t                            raw_data_count
) {
    const char* tag = BASE_TAG "/transmit";

    if (!self || !self->ctx || !execution_id || execution_id[0] == '\0') {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    app_internal_infrared_impl_ctx_t* ctx = self->ctx;

    if (!raw_data || raw_data_count == 0 || raw_data_count > IR_RAW_DATA_MAX_LEN) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Invalid raw_data payload");
        (void)ctx->cfg.def_pub->ir_transmit_ack(ctx->cfg.def_pub, ctx->device_id_str, execution_id, IR_TRANSMIT_ACK_FAILED, "invalid raw_data payload");
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    dom_models_ir_duration_t durations[IR_RAW_DATA_MAX_LEN];
    dom_models_error_t       err = app_internal_infrared_impl_raw_data_to_durations(raw_data, raw_data_count, durations);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Invalid raw_data payload: %s (%d)", dom_models_error_str(err), (int)err);
        (void)ctx->cfg.def_pub->ir_transmit_ack(ctx->cfg.def_pub, ctx->device_id_str, execution_id, IR_TRANSMIT_ACK_FAILED, "invalid raw_data payload");
        return err;
    }

    err = ctx->cfg.ir->transmit(ctx->cfg.ir, durations, raw_data_count);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "IR transmit failed: %s (%d)", dom_models_error_str(err), (int)err);
        (void)ctx->cfg.def_pub->ir_transmit_ack(ctx->cfg.def_pub, ctx->device_id_str, execution_id, IR_TRANSMIT_ACK_FAILED, dom_models_error_str(err));
        return err;
    }

    err = ctx->cfg.def_pub->ir_transmit_ack(ctx->cfg.def_pub, ctx->device_id_str, execution_id, IR_TRANSMIT_ACK_SUCCESS, NULL);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to publish ir/tx_ack: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "IR transmit completed successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static void on_ir_receive(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    void*                           cb_ctx
) {
    app_internal_infrared_impl_ctx_t* ctx = (app_internal_infrared_impl_ctx_t*)cb_ctx;
    if (!ctx || !durations || duration_count == 0 || duration_count > IR_RAW_DATA_MAX_LEN) {
        return;
    }

    int32_t raw_data[IR_RAW_DATA_MAX_LEN];
    if (app_internal_infrared_impl_durations_to_raw_data(durations, duration_count, raw_data) != DOMAIN_MODELS_ERROR_OK) {
        return;
    }

    /* Best-effort: this runs from the RX worker task, not a caller waiting
       on a return value - matches messaging_callbacks_impl.c's
       on_log_message, which also fires-and-forgets its def_pub call. */
    (void)ctx->cfg.def_pub->ir_capture(ctx->cfg.def_pub, ctx->device_id_str, raw_data, duration_count);
}
