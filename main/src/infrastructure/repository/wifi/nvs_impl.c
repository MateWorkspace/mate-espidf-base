#include "infrastructure/repository/wifi/nvs_impl.h"

#include <stdlib.h>
#include <string.h>

#include "domain/contracts/repository/wifi.h"
#include "domain/models/wifi.h"
#include "infrastructure/repository/wifi/nvs_impl_types.h"
#include "infrastructure/repository/wifi/nvs_impl_utils.h"

/* Contract Function Prototypes */

static dom_models_error_t get_sta_credential_impl(
    dom_contracts_repository_wifi_t*      self,
    dom_models_wifi_sta_connect_config_t* out
);
static dom_models_error_t set_sta_credential_impl(
    dom_contracts_repository_wifi_t*            self,
    const dom_models_wifi_sta_connect_config_t* credential
);
static dom_models_error_t clear_sta_credential_impl(
    dom_contracts_repository_wifi_t* self
);

/* Constructor and Destructor */

dom_contracts_repository_wifi_t* inf_repository_wifi_nvs_impl_new(const inf_repository_wifi_nvs_impl_cfg_t* cfg) {
    if (!cfg || !cfg->nvs) {
        return NULL;
    }

    inf_repository_wifi_nvs_impl_ctx_t* ctx = (inf_repository_wifi_nvs_impl_ctx_t*)calloc(1, sizeof(inf_repository_wifi_nvs_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(inf_repository_wifi_nvs_impl_cfg_t));

    dom_contracts_repository_wifi_t* self = dom_contracts_repository_wifi_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->get_sta_credential   = get_sta_credential_impl;
    self->set_sta_credential   = set_sta_credential_impl;
    self->clear_sta_credential = clear_sta_credential_impl;

    return self;
}

void inf_repository_wifi_nvs_impl_delete(dom_contracts_repository_wifi_t* self) {
    if (!self) {
        return;
    }

    free(self->ctx);
    dom_contracts_repository_wifi_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t get_sta_credential_impl(
    dom_contracts_repository_wifi_t*      self,
    dom_models_wifi_sta_connect_config_t* out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_wifi_nvs_impl_ctx_t* ctx = self->ctx;

    memset(out, 0, sizeof(dom_models_wifi_sta_connect_config_t));

    dom_models_error_t err = inf_repository_wifi_nvs_impl_read_string(
        ctx->cfg.nvs,
        DOM_MODELS_WIFI_STA_CREDENTIAL_SSID_KEY,
        out->ssid,
        sizeof(out->ssid)
    );
    if (err != DOMAIN_MODELS_ERROR_OK) {
        memset(out, 0, sizeof(dom_models_wifi_sta_connect_config_t));
        return err;
    }

    err = inf_repository_wifi_nvs_impl_read_string(
        ctx->cfg.nvs,
        DOM_MODELS_WIFI_STA_CREDENTIAL_PASS_KEY,
        out->password,
        sizeof(out->password)
    );
    if (err != DOMAIN_MODELS_ERROR_OK) {
        memset(out, 0, sizeof(dom_models_wifi_sta_connect_config_t));
        return err;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t set_sta_credential_impl(
    dom_contracts_repository_wifi_t*            self,
    const dom_models_wifi_sta_connect_config_t* credential
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_wifi_nvs_impl_ctx_t* ctx = self->ctx;

    return inf_repository_wifi_nvs_impl_set_credential(ctx->cfg.nvs, credential);
}

static dom_models_error_t clear_sta_credential_impl(
    dom_contracts_repository_wifi_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_wifi_nvs_impl_ctx_t* ctx = self->ctx;

    return inf_repository_wifi_nvs_impl_clear_credential(ctx->cfg.nvs);
}
