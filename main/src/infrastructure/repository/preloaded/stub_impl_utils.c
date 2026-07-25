#include "infrastructure/repository/preloaded/stub_impl_utils.h"

#include <stdlib.h>
#include <string.h>

dom_models_error_t inf_repository_preloaded_stub_impl_copy_cstr(char* out, size_t out_size, const char* value) {
    if (!out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    const char* src = value ? value : "";
    size_t      len = strlen(src);
    if (len >= out_size) {
        len = out_size - 1;
    }

    memcpy(out, src, len);
    out[len] = '\0';

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_repository_preloaded_stub_impl_set_string(char** runtime_value, const char* value) {
    if (!runtime_value || !value) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    size_t len  = strlen(value) + 1;
    char*  next = (char*)malloc(len);
    if (!next) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    memcpy(next, value, len);

    free(*runtime_value);
    *runtime_value = next;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_repository_preloaded_stub_impl_load_cfg(inf_repository_preloaded_stub_impl_ctx_t* ctx, const inf_repository_preloaded_stub_impl_cfg_t* cfg) {
    if (!ctx || !cfg) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_clear(ctx);

    ctx->device_id = cfg->device_id;

    dom_models_error_t err = inf_repository_preloaded_stub_impl_set_string(&ctx->device_id_str, cfg->device_id_str);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        inf_repository_preloaded_stub_impl_clear(ctx);
        return err;
    }

    err = inf_repository_preloaded_stub_impl_set_string(&ctx->mqtt_proto, cfg->mqtt_proto);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        inf_repository_preloaded_stub_impl_clear(ctx);
        return err;
    }

    err = inf_repository_preloaded_stub_impl_set_string(&ctx->mqtt_host, cfg->mqtt_host);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        inf_repository_preloaded_stub_impl_clear(ctx);
        return err;
    }

    err = inf_repository_preloaded_stub_impl_set_string(&ctx->mqtt_port, cfg->mqtt_port);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        inf_repository_preloaded_stub_impl_clear(ctx);
        return err;
    }

    err = inf_repository_preloaded_stub_impl_set_string(&ctx->mqtt_user, cfg->mqtt_user);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        inf_repository_preloaded_stub_impl_clear(ctx);
        return err;
    }

    err = inf_repository_preloaded_stub_impl_set_string(&ctx->mqtt_pass, cfg->mqtt_pass);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        inf_repository_preloaded_stub_impl_clear(ctx);
        return err;
    }

    ctx->system_restart_after_ms = cfg->system_restart_after_ms;

    return DOMAIN_MODELS_ERROR_OK;
}

void inf_repository_preloaded_stub_impl_clear(inf_repository_preloaded_stub_impl_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    free(ctx->device_id_str);
    free(ctx->mqtt_proto);
    free(ctx->mqtt_host);
    free(ctx->mqtt_port);
    free(ctx->mqtt_user);
    free(ctx->mqtt_pass);

    memset(ctx, 0, sizeof(inf_repository_preloaded_stub_impl_ctx_t));
}
