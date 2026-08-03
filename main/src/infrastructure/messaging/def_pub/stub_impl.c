#include "infrastructure/messaging/def_pub/stub_impl.h"

#include <stdlib.h>

#include "domain/contracts/messaging/def_pub.h"
#include "domain/models/device_status.h"
#include "domain/models/error.h"
#include "infrastructure/messaging/def_pub/stub_impl_utils.h"

/* Contract Function Prototypes */

static dom_models_error_t is_connected_impl(
    dom_contracts_messaging_def_pub_t* self,
    bool*                              out
);
static dom_models_error_t registration_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        device_info,
    const char*                        firmware_name,
    const dom_models_preloaded_kv_t*   config,
    size_t                             config_count
);
static dom_models_error_t status_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    dom_models_device_status_t         status
);
static dom_models_error_t log_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        msg,
    size_t                             msg_len
);
static dom_models_error_t action_ack_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        execution_id,
    const char*                        status,
    const char*                        message
);
static dom_models_error_t telemetry_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        metric_name,
    const char*                        payload_schema_name,
    int                                payload_schema_version,
    const char*                        payload_json
);

/* Constructor and Destructor */

dom_contracts_messaging_def_pub_t* inf_messaging_def_pub_stub_impl_new(
    const inf_messaging_def_pub_stub_impl_cfg_t* cfg
) {
    inf_messaging_def_pub_stub_impl_ctx_t* ctx = (inf_messaging_def_pub_stub_impl_ctx_t*)calloc(1, sizeof(inf_messaging_def_pub_stub_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    inf_messaging_def_pub_stub_impl_cfg_t default_cfg = INF_MESSAGING_DEF_PUB_STUB_IMPL_CFG_DEFAULT();
    dom_models_error_t                    err         = inf_messaging_def_pub_stub_impl_load_cfg(ctx, cfg ? cfg : &default_cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        free(ctx);
        return NULL;
    }

    dom_contracts_messaging_def_pub_t* self = dom_contracts_messaging_def_pub_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->is_connected = is_connected_impl;
    self->registration = registration_impl;
    self->status       = status_impl;
    self->log          = log_impl;
    self->action_ack   = action_ack_impl;
    self->telemetry    = telemetry_impl;

    return self;
}

void inf_messaging_def_pub_stub_impl_delete(dom_contracts_messaging_def_pub_t* self) {
    if (!self) {
        return;
    }

    free(self->ctx);
    dom_contracts_messaging_def_pub_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t is_connected_impl(
    dom_contracts_messaging_def_pub_t* self,
    bool*                              out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_messaging_def_pub_stub_impl_ctx_t* ctx = self->ctx;
    *out                                       = ctx->connected;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t registration_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        device_info,
    const char*                        firmware_name,
    const dom_models_preloaded_kv_t*   config,
    size_t                             config_count
) {
    (void)config;
    (void)config_count;

    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_pub_stub_impl_set_registration(self->ctx, device_id, device_info, firmware_name);
}

static dom_models_error_t status_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    dom_models_device_status_t         status
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_pub_stub_impl_set_status(self->ctx, device_id, status);
}

static dom_models_error_t log_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        msg,
    size_t                             msg_len
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_pub_stub_impl_set_log(self->ctx, device_id, msg, msg_len);
}

static dom_models_error_t action_ack_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        execution_id,
    const char*                        status,
    const char*                        message
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_pub_stub_impl_set_action_ack(self->ctx, device_id, execution_id, status, message);
}

static dom_models_error_t telemetry_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        metric_name,
    const char*                        payload_schema_name,
    int                                payload_schema_version,
    const char*                        payload_json
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_pub_stub_impl_set_telemetry(self->ctx, device_id, metric_name, payload_schema_name, payload_schema_version, payload_json);
}
