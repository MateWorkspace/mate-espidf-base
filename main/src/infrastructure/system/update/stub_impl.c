#include "infrastructure/system/update/stub_impl.h"

#include <stdlib.h>

#include "domain/contracts/system/update.h"
#include "domain/models/error.h"
#include "domain/models/update.h"
#include "infrastructure/system/update/stub_impl_utils.h"

/* Helper Function Prototypes */

static void dispatch_event(
    inf_system_update_stub_impl_ctx_t* ctx,
    dom_models_update_event_type_t     type,
    dom_models_error_t                 result,
    size_t                             bytes_written,
    size_t                             total_bytes
);

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
static dom_models_error_t add_event_callback_impl(
    dom_contracts_system_update_t*     self,
    void*                              cb_ctx,
    dom_models_update_event_callback_t cb_func
);
static dom_models_error_t remove_event_callback_impl(
    dom_contracts_system_update_t*     self,
    dom_models_update_event_callback_t cb_func
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

    self->update                = update_impl;
    self->validate              = validate_impl;
    self->rollback              = rollback_impl;
    self->add_event_callback    = add_event_callback_impl;
    self->remove_event_callback = remove_event_callback_impl;

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

    dispatch_event(ctx, DOM_MODELS_UPDATE_EVENT_PROGRESS, DOMAIN_MODELS_ERROR_OK, update_info->firmware_size, update_info->firmware_size);
    dispatch_event(ctx, DOM_MODELS_UPDATE_EVENT_COMPLETED, ctx->update_result, 0, 0);

    return DOMAIN_MODELS_ERROR_OK;
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

static dom_models_error_t add_event_callback_impl(
    dom_contracts_system_update_t*     self,
    void*                              cb_ctx,
    dom_models_update_event_callback_t cb_func
) {
    if (!self || !self->ctx || !cb_func) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_system_update_stub_impl_ctx_t* ctx = self->ctx;

    for (size_t i = 0; i < ctx->event_cb_cnt; i++) {
        if (ctx->event_cb_funcs[i] == cb_func) {
            return DOMAIN_MODELS_ERROR_OK;
        }
    }

    if (ctx->event_cb_cnt >= INF_SYSTEM_UPDATE_STUB_IMPL_EVENT_CALLBACK_MAX) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    ctx->event_cb_funcs[ctx->event_cb_cnt] = cb_func;
    ctx->event_cb_ctxs[ctx->event_cb_cnt]  = cb_ctx;
    ctx->event_cb_cnt += 1;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t remove_event_callback_impl(
    dom_contracts_system_update_t*     self,
    dom_models_update_event_callback_t cb_func
) {
    if (!self || !self->ctx || !cb_func) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_system_update_stub_impl_ctx_t* ctx = self->ctx;

    for (size_t i = 0; i < ctx->event_cb_cnt; i++) {
        if (ctx->event_cb_funcs[i] != cb_func) {
            continue;
        }

        size_t last_idx = ctx->event_cb_cnt - 1;

        ctx->event_cb_funcs[i] = NULL;
        ctx->event_cb_ctxs[i]  = NULL;

        if (i != last_idx) {
            ctx->event_cb_funcs[i] = ctx->event_cb_funcs[last_idx];
            ctx->event_cb_ctxs[i]  = ctx->event_cb_ctxs[last_idx];

            ctx->event_cb_funcs[last_idx] = NULL;
            ctx->event_cb_ctxs[last_idx]  = NULL;
        }

        ctx->event_cb_cnt -= 1;

        return DOMAIN_MODELS_ERROR_OK;
    }

    return DOMAIN_MODELS_ERROR_NOT_FOUND;
}

/* Helper Function Implementations */

static void dispatch_event(
    inf_system_update_stub_impl_ctx_t* ctx,
    dom_models_update_event_type_t     type,
    dom_models_error_t                 result,
    size_t                             bytes_written,
    size_t                             total_bytes
) {
    if (!ctx) {
        return;
    }

    dom_models_update_event_t event = {
        .type          = type,
        .result        = result,
        .bytes_written = bytes_written,
        .total_bytes   = total_bytes,
    };

    size_t cb_cnt = ctx->event_cb_cnt;
    for (size_t i = 0; i < cb_cnt; i++) {
        if (!ctx->event_cb_funcs[i]) {
            continue;
        }

        ctx->event_cb_funcs[i](ctx->event_cb_ctxs[i], &event);
    }
}
