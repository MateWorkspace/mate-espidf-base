#include "infrastructure/messaging/def_pub/stub_impl_utils.h"

#include <stdbool.h>
#include <string.h>

/* Helper Function Prototypes */

static bool cstr_available(const char* value);

static void copy_cstr(char* dst, size_t dst_size, const char* src);

dom_models_error_t inf_messaging_def_pub_stub_impl_load_cfg(
    inf_messaging_def_pub_stub_impl_ctx_t*       ctx,
    const inf_messaging_def_pub_stub_impl_cfg_t* cfg
) {
    if (!ctx || !cfg) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(ctx, 0, sizeof(inf_messaging_def_pub_stub_impl_ctx_t));

    ctx->connected                = cfg->connected;
    ctx->registration_publish_cnt = 0;
    ctx->status_publish_cnt       = 0;
    ctx->log_publish_cnt          = 0;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_pub_stub_impl_set_registration(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const char*                            device_info,
    const char*                            firmware_name
) {
    if (!ctx || !cstr_available(device_id) || !cstr_available(device_info) || !cstr_available(firmware_name)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    copy_cstr(ctx->last_registration_device_id, sizeof(ctx->last_registration_device_id), device_id);
    copy_cstr(ctx->last_registration_device_info, sizeof(ctx->last_registration_device_info), device_info);
    copy_cstr(ctx->last_registration_firmware_name, sizeof(ctx->last_registration_firmware_name), firmware_name);
    ctx->registration_publish_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_pub_stub_impl_set_status(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    dom_models_device_status_t             status
) {
    if (!ctx || !cstr_available(device_id)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    copy_cstr(ctx->last_status_device_id, sizeof(ctx->last_status_device_id), device_id);
    ctx->last_status = status;
    ctx->status_publish_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_pub_stub_impl_set_log(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const char*                            msg,
    size_t                                 msg_len
) {
    if (!ctx || !cstr_available(device_id) || !msg || msg_len == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    copy_cstr(ctx->last_log_device_id, sizeof(ctx->last_log_device_id), device_id);

    size_t copy_len = msg_len < sizeof(ctx->last_log_message) - 1 ? msg_len : sizeof(ctx->last_log_message) - 1;
    memcpy(ctx->last_log_message, msg, copy_len);
    ctx->last_log_message[copy_len] = '\0';
    ctx->log_publish_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_pub_stub_impl_set_action_ack(
    inf_messaging_def_pub_stub_impl_ctx_t* ctx,
    const char*                            device_id,
    const char*                            execution_id,
    const char*                            status,
    const char*                            message
) {
    if (!ctx || !cstr_available(device_id) || !cstr_available(execution_id) || !cstr_available(status)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    copy_cstr(ctx->last_action_ack_device_id, sizeof(ctx->last_action_ack_device_id), device_id);
    copy_cstr(ctx->last_action_ack_execution_id, sizeof(ctx->last_action_ack_execution_id), execution_id);
    copy_cstr(ctx->last_action_ack_status, sizeof(ctx->last_action_ack_status), status);
    copy_cstr(ctx->last_action_ack_message, sizeof(ctx->last_action_ack_message), message);
    ctx->action_ack_publish_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static bool cstr_available(const char* value) {
    return value && value[0] != '\0';
}

static void copy_cstr(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) {
        return;
    }

    const char* value = src ? src : "";
    size_t      len   = strlen(value);
    if (len >= dst_size) {
        len = dst_size - 1;
    }

    memcpy(dst, value, len);
    dst[len] = '\0';
}
