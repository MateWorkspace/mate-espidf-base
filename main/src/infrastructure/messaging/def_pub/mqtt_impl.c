#include "infrastructure/messaging/def_pub/mqtt_impl.h"

#include <stdlib.h>
#include <string.h>

#include "domain/contracts/messaging/def_pub.h"
#include "domain/models/device_status.h"
#include "domain/models/error.h"
#include "infrastructure/messaging/def_pub/mqtt_impl_utils.h"

/* Contract Function Prototypes */

static dom_models_error_t is_connected_impl(
    dom_contracts_messaging_def_pub_t* self,
    bool*                              out
);
static dom_models_error_t registration_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        device_info,
    const char*                        firmware_name
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

/* Event Handler for connection tracking */

static void esp_mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    (void)base;
    (void)event_data;

    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = (inf_messaging_def_pub_mqtt_impl_ctx_t*)handler_args;
    if (!ctx) {
        return;
    }

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ctx->connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ctx->connected = false;
            break;
        default:
            break;
    }
}

/* Constructor and Destructor */

dom_contracts_messaging_def_pub_t* inf_messaging_def_pub_mqtt_impl_new(
    const inf_messaging_def_pub_mqtt_impl_cfg_t* cfg
) {
    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = (inf_messaging_def_pub_mqtt_impl_ctx_t*)calloc(1, sizeof(inf_messaging_def_pub_mqtt_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    inf_messaging_def_pub_mqtt_impl_cfg_t default_cfg = INF_MESSAGING_DEF_PUB_MQTT_IMPL_CFG_DEFAULT();
    memcpy(&ctx->cfg, cfg ? cfg : &default_cfg, sizeof(inf_messaging_def_pub_mqtt_impl_cfg_t));

    dom_models_error_t err = inf_messaging_def_pub_mqtt_impl_validate_cfg(&ctx->cfg);
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

    esp_err_t event_err = esp_mqtt_client_register_event(
        ctx->cfg.mqtt_client,
        ESP_EVENT_ANY_ID,
        esp_mqtt_event_handler,
        ctx
    );
    if (event_err != ESP_OK) {
        dom_contracts_messaging_def_pub_delete(self);
        free(ctx);
        return NULL;
    }

    return self;
}

void inf_messaging_def_pub_mqtt_impl_delete(dom_contracts_messaging_def_pub_t* self) {
    if (!self) {
        return;
    }

    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        if (ctx->cfg.mqtt_client) {
            esp_mqtt_client_unregister_event(
                ctx->cfg.mqtt_client,
                ESP_EVENT_ANY_ID,
                esp_mqtt_event_handler
            );
        }
        free(ctx);
    }

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

    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = self->ctx;
    *out                                       = ctx->connected;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t registration_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        device_info,
    const char*                        firmware_name
) {
    if (!self || !self->ctx || !device_id || device_id[0] == '\0' || !device_info || device_info[0] == '\0' || !firmware_name || firmware_name[0] == '\0') {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = self->ctx;

    return inf_messaging_def_pub_mqtt_impl_publish_json(
        ctx,
        "/pub/registration",
        inf_messaging_def_pub_mqtt_impl_build_registration_json(device_id, device_info, firmware_name),
        INF_MESSAGING_DEF_PUB_MQTT_IMPL_QOS_DEFAULT,
        false
    );
}

static dom_models_error_t status_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    dom_models_device_status_t         status
) {
    if (!self || !self->ctx || !device_id || device_id[0] == '\0') {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = self->ctx;

    char               topic[INF_MESSAGING_DEF_PUB_MQTT_IMPL_TOPIC_MAX_LEN];
    dom_models_error_t err = inf_messaging_def_pub_mqtt_impl_build_device_topic(device_id, "status", topic, sizeof(topic));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return inf_messaging_def_pub_mqtt_impl_publish_json(
        ctx,
        topic,
        inf_messaging_def_pub_mqtt_impl_build_status_json(status),
        INF_MESSAGING_DEF_PUB_MQTT_IMPL_QOS_DEFAULT,
        true /* retained: mirrors the LWT's OFFLINE retain so a fresh
                subscriber (e.g. the backend after its own restart) gets
                the device's current status immediately, not just future
                updates */
    );
}

static dom_models_error_t log_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        msg,
    size_t                             msg_len
) {
    if (!self || !self->ctx || !device_id || device_id[0] == '\0' || !msg || msg_len == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = self->ctx;

    char               topic[INF_MESSAGING_DEF_PUB_MQTT_IMPL_TOPIC_MAX_LEN];
    dom_models_error_t err = inf_messaging_def_pub_mqtt_impl_build_device_topic(device_id, "log", topic, sizeof(topic));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return inf_messaging_def_pub_mqtt_impl_publish_raw(
        ctx,
        topic,
        msg,
        msg_len,
        INF_MESSAGING_DEF_PUB_MQTT_IMPL_QOS_LOG
    );
}

static dom_models_error_t action_ack_impl(
    dom_contracts_messaging_def_pub_t* self,
    const char*                        device_id,
    const char*                        execution_id,
    const char*                        status,
    const char*                        message
) {
    if (!self || !self->ctx || !device_id || device_id[0] == '\0' || !execution_id || execution_id[0] == '\0' || !status || status[0] == '\0') {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_messaging_def_pub_mqtt_impl_ctx_t* ctx = self->ctx;

    char               topic[INF_MESSAGING_DEF_PUB_MQTT_IMPL_TOPIC_MAX_LEN];
    dom_models_error_t err = inf_messaging_def_pub_mqtt_impl_build_device_topic(device_id, "action_ack", topic, sizeof(topic));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return inf_messaging_def_pub_mqtt_impl_publish_json(
        ctx,
        topic,
        inf_messaging_def_pub_mqtt_impl_build_action_ack_json(execution_id, status, message),
        INF_MESSAGING_DEF_PUB_MQTT_IMPL_QOS_DEFAULT,
        false
    );
}
