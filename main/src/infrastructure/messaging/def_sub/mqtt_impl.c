#include "infrastructure/messaging/def_sub/mqtt_impl.h"

#include <stdlib.h>
#include <string.h>

#include "domain/contracts/messaging/def_sub.h"
#include "domain/models/error.h"
#include "infrastructure/messaging/def_sub/mqtt_impl_utils.h"

/* Contract Function Prototypes */

static dom_models_error_t registration_ack_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
);
static dom_models_error_t ota_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
);
static dom_models_error_t action_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
);
static dom_models_error_t config_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
);

/* Constructor and Destructor */

dom_contracts_messaging_def_sub_t* inf_messaging_def_sub_mqtt_impl_new(
    const inf_messaging_def_sub_mqtt_impl_cfg_t* cfg
) {
    inf_messaging_def_sub_mqtt_impl_ctx_t* ctx = (inf_messaging_def_sub_mqtt_impl_ctx_t*)calloc(1, sizeof(inf_messaging_def_sub_mqtt_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    inf_messaging_def_sub_mqtt_impl_cfg_t default_cfg = INF_MESSAGING_DEF_SUB_MQTT_IMPL_CFG_DEFAULT();
    memcpy(&ctx->cfg, cfg ? cfg : &default_cfg, sizeof(inf_messaging_def_sub_mqtt_impl_cfg_t));

    dom_models_error_t err = inf_messaging_def_sub_mqtt_impl_validate_cfg(&ctx->cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        free(ctx);
        return NULL;
    }

    dom_contracts_messaging_def_sub_t* self = dom_contracts_messaging_def_sub_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->registration_ack = registration_ack_impl;
    self->ota              = ota_impl;
    self->action           = action_impl;
    self->config           = config_impl;

    return self;
}

void inf_messaging_def_sub_mqtt_impl_delete(dom_contracts_messaging_def_sub_t* self) {
    if (!self) {
        return;
    }

    free(self->ctx);
    dom_contracts_messaging_def_sub_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t registration_ack_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_sub_mqtt_impl_subscribe_suffix(self->ctx, device_id, "registration_ack");
}

static dom_models_error_t ota_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_sub_mqtt_impl_subscribe_suffix(self->ctx, device_id, "ota");
}

static dom_models_error_t action_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_sub_mqtt_impl_subscribe_suffix(self->ctx, device_id, "action");
}

static dom_models_error_t config_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_sub_mqtt_impl_subscribe_suffix(self->ctx, device_id, "config");
}
