#include "application/internal/log_forwarding/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/log_forwarding/impl_types.h"
#include "application/internal/log_forwarding/impl_utils.h"
#include "domain/models/error.h"
#include "domain/usecases/internal/log_forwarding.h"

#define BASE_TAG "internal_log_forwarding"

/* Helper Function Prototypes */

static dom_models_error_t get_ctx(
    dom_usecases_internal_log_forwarding_t*  self,
    app_internal_log_forwarding_impl_ctx_t** out
);

static void on_log_message(void* cb_ctx, const char* msg, size_t msg_len);

/* Contract Function Prototypes */

static dom_models_error_t add_sink_impl(
    dom_usecases_internal_log_forwarding_t* self,
    void*                                   cb_ctx,
    dom_contracts_logger_leveled_cb         cb_func
);
static dom_models_error_t remove_sink_impl(
    dom_usecases_internal_log_forwarding_t* self,
    dom_contracts_logger_leveled_cb         cb_func
);

/* Constructor and Destructor */

dom_usecases_internal_log_forwarding_t* app_internal_log_forwarding_impl_new(const app_internal_log_forwarding_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    dom_models_error_t err = app_internal_log_forwarding_impl_validate_cfg(cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_log_forwarding_impl_ctx_t* ctx = (app_internal_log_forwarding_impl_ctx_t*)calloc(1, sizeof(app_internal_log_forwarding_impl_ctx_t));
    if (!ctx) {
        cfg->logger->error(cfg->logger, tag, "Failed to allocate log forwarding context: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_log_forwarding_impl_cfg_t));

    dom_usecases_internal_log_forwarding_t* self = dom_usecases_internal_log_forwarding_new(ctx);
    if (!self) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to allocate log forwarding usecase: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        free(ctx);
        return NULL;
    }

    self->add_sink    = add_sink_impl;
    self->remove_sink = remove_sink_impl;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Log forwarding created successfully");

    return self;
}

void app_internal_log_forwarding_impl_delete(dom_usecases_internal_log_forwarding_t* self) {
    const char* tag = BASE_TAG "/delete";

    if (!self) {
        return;
    }

    app_internal_log_forwarding_impl_deinit(self);

    app_internal_log_forwarding_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Log forwarding deleted successfully");
        free(ctx);
    }

    dom_usecases_internal_log_forwarding_delete(self);
}

dom_models_error_t app_internal_log_forwarding_impl_init(dom_usecases_internal_log_forwarding_t* self) {
    const char* tag = BASE_TAG "/init";

    app_internal_log_forwarding_impl_ctx_t* ctx = NULL;
    dom_models_error_t                      err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (ctx->subscribed) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    err = ctx->cfg.logger->add_callback(ctx->cfg.logger, ctx, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to logger callbacks: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->subscribed = true;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Log forwarding initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void app_internal_log_forwarding_impl_deinit(dom_usecases_internal_log_forwarding_t* self) {
    const char* tag = BASE_TAG "/deinit";

    app_internal_log_forwarding_impl_ctx_t* ctx = NULL;
    if (get_ctx(self, &ctx) != DOMAIN_MODELS_ERROR_OK || !ctx->subscribed) {
        return;
    }

    ctx->cfg.logger->remove_callback(ctx->cfg.logger, on_log_message);
    ctx->subscribed = false;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Log forwarding deinitialized successfully");
}

/* Contract Function Implementations */

static dom_models_error_t add_sink_impl(
    dom_usecases_internal_log_forwarding_t* self,
    void*                                   cb_ctx,
    dom_contracts_logger_leveled_cb         cb_func
) {
    const char* tag = BASE_TAG "/add_sink";

    app_internal_log_forwarding_impl_ctx_t* ctx = NULL;
    dom_models_error_t                      err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!cb_func) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing sink callback function: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    if (ctx->sink_cb_idx >= APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_SINK_CB_MAX_CNT) {
        err = DOMAIN_MODELS_ERROR_BAD_STATE;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "No room left for another log forwarding sink: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->sink_cb_funcs[ctx->sink_cb_idx] = cb_func;
    ctx->sink_cb_ctxs[ctx->sink_cb_idx]  = cb_ctx;
    ctx->sink_cb_idx += 1;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t remove_sink_impl(
    dom_usecases_internal_log_forwarding_t* self,
    dom_contracts_logger_leveled_cb         cb_func
) {
    app_internal_log_forwarding_impl_ctx_t* ctx = NULL;
    dom_models_error_t                      err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!cb_func || ctx->sink_cb_idx == 0) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    for (unsigned int i = 0; i < ctx->sink_cb_idx; i++) {
        if (ctx->sink_cb_funcs[i] != cb_func) {
            continue;
        }

        unsigned int last_idx = ctx->sink_cb_idx - 1;

        ctx->sink_cb_funcs[i] = NULL;
        ctx->sink_cb_ctxs[i]  = NULL;

        if (i != last_idx) {
            ctx->sink_cb_funcs[i] = ctx->sink_cb_funcs[last_idx];
            ctx->sink_cb_ctxs[i]  = ctx->sink_cb_ctxs[last_idx];

            ctx->sink_cb_funcs[last_idx] = NULL;
            ctx->sink_cb_ctxs[last_idx]  = NULL;
        }

        ctx->sink_cb_idx -= 1;
        return DOMAIN_MODELS_ERROR_OK;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static dom_models_error_t get_ctx(
    dom_usecases_internal_log_forwarding_t*  self,
    app_internal_log_forwarding_impl_ctx_t** out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = self->ctx;

    return DOMAIN_MODELS_ERROR_OK;
}

/* Runs synchronously on whichever task called logger->error/warn/info/
   debug(...) - never call back into ctx->cfg.logger here, since this
   callback is registered on that same logger instance and logging from
   within it would re-enter print_log() -> run_callbacks() -> this
   function, recursing without end (same rule messaging_callbacks'
   on_log_message documented before this usecase existed). */
static void on_log_message(void* cb_ctx, const char* msg, size_t msg_len) {
    app_internal_log_forwarding_impl_ctx_t* ctx = cb_ctx;
    if (!ctx || !msg || msg_len == 0) {
        return;
    }

    for (unsigned int i = 0; i < ctx->sink_cb_idx; i++) {
        if (ctx->sink_cb_funcs[i]) {
            ctx->sink_cb_funcs[i](ctx->sink_cb_ctxs[i], msg, msg_len);
        }
    }
}
