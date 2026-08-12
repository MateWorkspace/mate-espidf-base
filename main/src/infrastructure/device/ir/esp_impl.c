#include "infrastructure/device/ir/esp_impl.h"

#include <stdlib.h>

#include "domain/models/error.h"
#include "infrared.h"

#define BASE_TAG "inf_device_ir_esp_impl"

/* Contract Function Prototypes */

static dom_models_error_t transmit_impl(
    dom_contracts_device_ir_t*      self,
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count
);
static dom_models_error_t set_receive_handler_impl(
    dom_contracts_device_ir_t* self,
    void*                      cb_ctx,
    dom_models_ir_receive_cb_t cb
);

/* Helper Function Prototypes */

static dom_models_error_t map_error(esp_err_t err);

static void on_receive(const infrared_rx_frame_t* frame, void* user_ctx);

/* Constructor and Destructor */

dom_contracts_device_ir_t* inf_device_ir_esp_impl_new(void* unused_cfg) {
    (void)unused_cfg;

    inf_device_ir_esp_impl_ctx_t* ctx = (inf_device_ir_esp_impl_ctx_t*)calloc(1, sizeof(inf_device_ir_esp_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    dom_contracts_device_ir_t* self = dom_contracts_device_ir_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->transmit            = transmit_impl;
    self->set_receive_handler = set_receive_handler_impl;

    return self;
}

void inf_device_ir_esp_impl_delete(dom_contracts_device_ir_t* self) {
    if (!self) {
        return;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        if (ctx->infrared) {
            infrared_delete((infrared_handle_t*)ctx->infrared);
        }
        free(ctx);
    }

    dom_contracts_device_ir_delete(self);
}

/* Lifecycle */

dom_models_error_t inf_device_ir_esp_impl_init(dom_contracts_device_ir_t* self) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    if (ctx->initialized) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    infrared_cfg_t cfg = INFRARED_CFG_DEFAULT();
    cfg.rx_gpio        = INF_DEVICE_IR_ESP_IMPL_RX_GPIO;
    cfg.tx_gpio        = INF_DEVICE_IR_ESP_IMPL_TX_GPIO;
    cfg.enable_rx      = true;
    cfg.enable_tx      = true;
    cfg.on_receive     = on_receive;
    cfg.user_ctx       = ctx;

    infrared_handle_t* infrared = infrared_new(&cfg);
    if (!infrared) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    esp_err_t err = infrared_init(infrared);
    if (err != ESP_OK) {
        infrared_delete(infrared);
        return map_error(err);
    }

    ctx->infrared    = infrared;
    ctx->initialized = true;

    return DOMAIN_MODELS_ERROR_OK;
}

void inf_device_ir_esp_impl_deinit(dom_contracts_device_ir_t* self) {
    if (!self || !self->ctx) {
        return;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    if (!ctx->initialized) {
        return;
    }

    if (ctx->infrared) {
        infrared_deinit((infrared_handle_t*)ctx->infrared);
    }
    ctx->initialized = false;
}

/* Contract Function Implementations */

static dom_models_error_t transmit_impl(
    dom_contracts_device_ir_t*      self,
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count
) {
    if (!self || !self->ctx || !durations || duration_count == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    if (!ctx->initialized || !ctx->infrared) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    infrared_duration_t* converted = (infrared_duration_t*)calloc(duration_count, sizeof(infrared_duration_t));
    if (!converted) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    for (size_t i = 0; i < duration_count; i++) {
        converted[i].level       = durations[i].level;
        converted[i].duration_us = durations[i].duration_us;
    }

    esp_err_t err = infrared_transmit((infrared_handle_t*)ctx->infrared, converted, duration_count);
    free(converted);

    return map_error(err);
}

static dom_models_error_t set_receive_handler_impl(
    dom_contracts_device_ir_t* self,
    void*                      cb_ctx,
    dom_models_ir_receive_cb_t cb
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_ir_esp_impl_ctx_t* ctx = self->ctx;
    ctx->receive_cb                   = cb;
    ctx->receive_cb_ctx               = cb_ctx;

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static dom_models_error_t map_error(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return DOMAIN_MODELS_ERROR_OK;
        case ESP_ERR_INVALID_ARG:
            return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        case ESP_ERR_INVALID_STATE:
            return DOMAIN_MODELS_ERROR_BAD_STATE;
        case ESP_ERR_NO_MEM:
            return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
        case ESP_ERR_TIMEOUT:
            return DOMAIN_MODELS_ERROR_TIMEOUT;
        default:
            return DOMAIN_MODELS_ERROR_FAILURE;
    }
}

static void on_receive(const infrared_rx_frame_t* frame, void* user_ctx) {
    inf_device_ir_esp_impl_ctx_t* ctx = (inf_device_ir_esp_impl_ctx_t*)user_ctx;
    if (!ctx || !frame || !ctx->receive_cb) {
        return;
    }
    if (frame->overflowed || !frame->durations || frame->duration_count == 0) {
        return;
    }

    dom_models_ir_duration_t* converted = (dom_models_ir_duration_t*)calloc(frame->duration_count, sizeof(dom_models_ir_duration_t));
    if (!converted) {
        return;
    }

    for (size_t i = 0; i < frame->duration_count; i++) {
        converted[i].level       = frame->durations[i].level;
        converted[i].duration_us = frame->durations[i].duration_us;
    }

    ctx->receive_cb(converted, frame->duration_count, ctx->receive_cb_ctx);
    free(converted);
}
