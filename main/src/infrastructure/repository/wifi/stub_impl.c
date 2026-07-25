#include "infrastructure/repository/wifi/stub_impl.h"

#include <stdlib.h>
#include <string.h>

#include "domain/contracts/repository/wifi.h"
#include "domain/models/wifi.h"
#include "infrastructure/repository/wifi/stub_impl_types.h"
#include "infrastructure/repository/wifi/stub_impl_utils.h"

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

dom_contracts_repository_wifi_t* inf_repository_wifi_stub_impl_new(const inf_repository_wifi_stub_impl_cfg_t* cfg) {
    inf_repository_wifi_stub_impl_ctx_t* ctx = (inf_repository_wifi_stub_impl_ctx_t*)calloc(1, sizeof(inf_repository_wifi_stub_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    inf_repository_wifi_stub_impl_cfg_t default_cfg = INF_REPOSITORY_WIFI_STUB_IMPL_CFG_DEFAULT();
    dom_models_error_t                  err         = inf_repository_wifi_stub_impl_load_cfg(ctx, cfg ? cfg : &default_cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        free(ctx);
        return NULL;
    }

    dom_contracts_repository_wifi_t* self = dom_contracts_repository_wifi_new(ctx);
    if (!self) {
        inf_repository_wifi_stub_impl_clear(ctx);
        free(ctx);
        return NULL;
    }

    self->get_sta_credential   = get_sta_credential_impl;
    self->set_sta_credential   = set_sta_credential_impl;
    self->clear_sta_credential = clear_sta_credential_impl;

    return self;
}

void inf_repository_wifi_stub_impl_delete(dom_contracts_repository_wifi_t* self) {
    if (!self) {
        return;
    }

    inf_repository_wifi_stub_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        inf_repository_wifi_stub_impl_clear(ctx);
        free(ctx);
    }

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

    inf_repository_wifi_stub_impl_ctx_t* ctx = self->ctx;
    if (!ctx->credential_available) {
        memset(out, 0, sizeof(dom_models_wifi_sta_connect_config_t));
        return DOMAIN_MODELS_ERROR_NOT_FOUND;
    }

    memcpy(out, &ctx->credential, sizeof(dom_models_wifi_sta_connect_config_t));

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t set_sta_credential_impl(
    dom_contracts_repository_wifi_t*            self,
    const dom_models_wifi_sta_connect_config_t* credential
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_wifi_stub_impl_ctx_t* ctx = self->ctx;

    return inf_repository_wifi_stub_impl_set_credential(ctx, credential);
}

static dom_models_error_t clear_sta_credential_impl(
    dom_contracts_repository_wifi_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_wifi_stub_impl_ctx_t* ctx = self->ctx;

    inf_repository_wifi_stub_impl_clear(ctx);

    return DOMAIN_MODELS_ERROR_OK;
}
