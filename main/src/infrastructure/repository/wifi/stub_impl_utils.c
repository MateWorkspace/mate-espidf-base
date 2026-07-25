#include "infrastructure/repository/wifi/stub_impl_utils.h"

#include <string.h>

/* Helper Function Prototypes */

static size_t bounded_strlen(const char* value, size_t max_len);

dom_models_error_t inf_repository_wifi_stub_impl_validate_credential(const dom_models_wifi_sta_connect_config_t* credential) {
    if (!credential) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    size_t ssid_len = bounded_strlen(credential->ssid, sizeof(credential->ssid));
    if (ssid_len == 0 || ssid_len >= sizeof(credential->ssid)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    size_t password_len = bounded_strlen(credential->password, sizeof(credential->password));
    if (password_len >= sizeof(credential->password)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_repository_wifi_stub_impl_set_credential(
    inf_repository_wifi_stub_impl_ctx_t*        ctx,
    const dom_models_wifi_sta_connect_config_t* credential
) {
    if (!ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    dom_models_error_t err = inf_repository_wifi_stub_impl_validate_credential(credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    memcpy(&ctx->credential, credential, sizeof(dom_models_wifi_sta_connect_config_t));
    ctx->credential_available = true;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_repository_wifi_stub_impl_load_cfg(
    inf_repository_wifi_stub_impl_ctx_t*       ctx,
    const inf_repository_wifi_stub_impl_cfg_t* cfg
) {
    if (!ctx || !cfg) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_wifi_stub_impl_clear(ctx);

    if (!cfg->credential_available) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    return inf_repository_wifi_stub_impl_set_credential(ctx, &cfg->credential);
}

void inf_repository_wifi_stub_impl_clear(inf_repository_wifi_stub_impl_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    memset(ctx, 0, sizeof(inf_repository_wifi_stub_impl_ctx_t));
}

/* Helper Function Implementations */

static size_t bounded_strlen(const char* value, size_t max_len) {
    size_t len = 0;

    if (!value) {
        return 0;
    }

    while (len < max_len && value[len] != '\0') {
        len++;
    }

    return len;
}
