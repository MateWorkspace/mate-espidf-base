#include "infrastructure/system/update/stub_impl.h"

#include <stdlib.h>

#include "domain/contracts/system/update.h"
#include "domain/models/error.h"
#include "domain/models/update.h"
#include "infrastructure/system/update/stub_impl_utils.h"

/* Contract Function Prototypes */

static dom_models_error_t update_impl(
    dom_contracts_system_update_t*  self,
    const dom_models_update_info_t* update_info
);
static dom_models_error_t validate_impl(
    dom_contracts_system_update_t* self
);
static dom_models_error_t rollback_impl(
    dom_contracts_system_update_t* self
);

/* Constructor and Destructor */

dom_contracts_system_update_t* inf_system_update_stub_impl_new(
    const inf_system_update_stub_impl_cfg_t* cfg
) {
    inf_system_update_stub_impl_ctx_t* ctx = (inf_system_update_stub_impl_ctx_t*)calloc(1, sizeof(inf_system_update_stub_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    inf_system_update_stub_impl_cfg_t default_cfg = INF_SYSTEM_UPDATE_STUB_IMPL_CFG_DEFAULT();
    dom_models_error_t                err         = inf_system_update_stub_impl_load_cfg(ctx, cfg ? cfg : &default_cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        free(ctx);
        return NULL;
    }

    dom_contracts_system_update_t* self = dom_contracts_system_update_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->update   = update_impl;
    self->validate = validate_impl;
    self->rollback = rollback_impl;

    return self;
}

void inf_system_update_stub_impl_delete(dom_contracts_system_update_t* self) {
    if (!self) {
        return;
    }

    free(self->ctx);
    dom_contracts_system_update_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t update_impl(
    dom_contracts_system_update_t*  self,
    const dom_models_update_info_t* update_info
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_system_update_stub_impl_ctx_t* ctx = self->ctx;
    dom_models_error_t                 err = inf_system_update_stub_impl_set_update(ctx, update_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return ctx->update_result;
}

static dom_models_error_t validate_impl(
    dom_contracts_system_update_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_system_update_stub_impl_ctx_t* ctx = self->ctx;
    ctx->validate_cnt++;

    return ctx->validate_result;
}

static dom_models_error_t rollback_impl(
    dom_contracts_system_update_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_system_update_stub_impl_ctx_t* ctx = self->ctx;
    ctx->rollback_cnt++;

    return ctx->rollback_result;
}
