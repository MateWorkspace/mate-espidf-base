#include "infrastructure/messaging/def_sub/stub_impl_utils.h"

#include <stdbool.h>
#include <string.h>

/* Helper Function Prototypes */

static bool cstr_available(const char* value);

static void copy_cstr(char* dst, size_t dst_size, const char* src);

dom_models_error_t inf_messaging_def_sub_stub_impl_load_cfg(
    inf_messaging_def_sub_stub_impl_ctx_t*       ctx,
    const inf_messaging_def_sub_stub_impl_cfg_t* cfg
) {
    if (!ctx || !cfg) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(ctx, 0, sizeof(inf_messaging_def_sub_stub_impl_ctx_t));

    ctx->registration_ack_subscribed = cfg->registration_ack_subscribed;
    ctx->ota_subscribed              = cfg->ota_subscribed;
    ctx->action_subscribed           = cfg->action_subscribed;
    ctx->config_subscribed           = cfg->config_subscribed;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_registration_ack(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
) {
    if (!ctx || !cstr_available(device_id)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    ctx->registration_ack_subscribed = true;
    copy_cstr(ctx->last_registration_ack_device_id, sizeof(ctx->last_registration_ack_device_id), device_id);
    ctx->registration_ack_subscribe_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_ota(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
) {
    if (!ctx || !cstr_available(device_id)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    ctx->ota_subscribed = true;
    copy_cstr(ctx->last_ota_device_id, sizeof(ctx->last_ota_device_id), device_id);
    ctx->ota_subscribe_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_action(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
) {
    if (!ctx || !cstr_available(device_id)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    ctx->action_subscribed = true;
    copy_cstr(ctx->last_action_device_id, sizeof(ctx->last_action_device_id), device_id);
    ctx->action_subscribe_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_config(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
) {
    if (!ctx || !cstr_available(device_id)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    ctx->config_subscribed = true;
    copy_cstr(ctx->last_config_device_id, sizeof(ctx->last_config_device_id), device_id);
    ctx->config_subscribe_cnt++;

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
