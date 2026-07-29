#include "application/internal/ota/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/ota/impl_types.h"
#include "application/internal/ota/impl_utils.h"
#include "domain/models/error.h"
#include "domain/usecases/internal/ota.h"

#define BASE_TAG "internal_ota"

/* Helper Function Prototypes */

static dom_models_error_t get_ctx(
    dom_usecases_internal_ota_t*  self,
    app_internal_ota_impl_ctx_t** out
);

static void on_update_event(
    void*                            cb_ctx,
    const dom_models_update_event_t* event
);

/* Contract Function Prototypes */

static dom_models_error_t update_impl(
    dom_usecases_internal_ota_t*    self,
    const dom_models_update_info_t* update_info
);
static dom_models_error_t validate_impl(
    dom_usecases_internal_ota_t* self
);
static dom_models_error_t rollback_impl(
    dom_usecases_internal_ota_t* self
);
static dom_models_error_t get_status_impl(
    dom_usecases_internal_ota_t*        self,
    dom_usecases_internal_ota_status_t* out
);

/* Constructor and Destructor */

dom_usecases_internal_ota_t* app_internal_ota_impl_new(const app_internal_ota_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    dom_models_error_t err = app_internal_ota_impl_validate_cfg(cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_ota_impl_ctx_t* ctx = (app_internal_ota_impl_ctx_t*)calloc(1, sizeof(app_internal_ota_impl_ctx_t));
    if (!ctx) {
        cfg->logger->error(cfg->logger, tag, "Failed to allocate OTA context: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_ota_impl_cfg_t));
    if (ctx->cfg.restart_delay_ms == 0) {
        ctx->cfg.restart_delay_ms = APP_INTERNAL_OTA_IMPL_DEFAULT_RESTART_DELAY_MS;
    }
    ctx->updating    = false;
    ctx->last_result = DOMAIN_MODELS_ERROR_OK;

    dom_usecases_internal_ota_t* self = dom_usecases_internal_ota_new(ctx);
    if (!self) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to allocate OTA usecase: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        free(ctx);
        return NULL;
    }

    self->update     = update_impl;
    self->validate   = validate_impl;
    self->rollback   = rollback_impl;
    self->get_status = get_status_impl;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA created successfully");

    return self;
}

void app_internal_ota_impl_delete(dom_usecases_internal_ota_t* self) {
    const char* tag = BASE_TAG "/delete";

    if (!self) {
        return;
    }

    app_internal_ota_impl_deinit(self);

    app_internal_ota_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA deleted successfully");
        free(ctx);
    }

    dom_usecases_internal_ota_delete(self);
}

dom_models_error_t app_internal_ota_impl_init(dom_usecases_internal_ota_t* self) {
    const char* tag = BASE_TAG "/init";

    app_internal_ota_impl_ctx_t* ctx = NULL;
    dom_models_error_t           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (ctx->event_subscribed) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    err = ctx->cfg.system_update->add_event_callback(ctx->cfg.system_update, ctx, on_update_event);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to register update event callback: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->event_subscribed = true;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void app_internal_ota_impl_deinit(dom_usecases_internal_ota_t* self) {
    const char* tag = BASE_TAG "/deinit";

    app_internal_ota_impl_ctx_t* ctx = NULL;
    if (get_ctx(self, &ctx) != DOMAIN_MODELS_ERROR_OK || !ctx->event_subscribed) {
        return;
    }

    (void)ctx->cfg.system_update->remove_event_callback(ctx->cfg.system_update, on_update_event);
    ctx->event_subscribed = false;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA deinitialized successfully");
}

/* Contract Function Implementations */

static dom_models_error_t update_impl(
    dom_usecases_internal_ota_t*    self,
    const dom_models_update_info_t* update_info
) {
    const char* tag = BASE_TAG "/update";

    app_internal_ota_impl_ctx_t* ctx = NULL;
    dom_models_error_t           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = app_internal_ota_impl_validate_update_info(update_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Invalid update info: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    if (ctx->updating) {
        err = DOMAIN_MODELS_ERROR_BAD_STATE;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "OTA update already in progress: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->updating              = true;
    ctx->last_progress_percent = -1;

    err = ctx->cfg.system_update->update(ctx->cfg.system_update, update_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->updating = false;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to request OTA update: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA update requested successfully for URL: %s", update_info->firmware_url);

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t validate_impl(
    dom_usecases_internal_ota_t* self
) {
    const char* tag = BASE_TAG "/validate";

    app_internal_ota_impl_ctx_t* ctx = NULL;
    dom_models_error_t           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.system_update->validate(ctx->cfg.system_update);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to validate running partition: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Running partition validated successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t rollback_impl(
    dom_usecases_internal_ota_t* self
) {
    const char* tag = BASE_TAG "/rollback";

    app_internal_ota_impl_ctx_t* ctx = NULL;
    dom_models_error_t           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.system_update->rollback(ctx->cfg.system_update);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to perform rollback: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Rollback requested successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_status_impl(
    dom_usecases_internal_ota_t*        self,
    dom_usecases_internal_ota_status_t* out
) {
    const char* tag = BASE_TAG "/get_status";

    app_internal_ota_impl_ctx_t* ctx = NULL;
    dom_models_error_t           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing OTA status output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    out->updating    = ctx->updating;
    out->last_result = ctx->last_result;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA status retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static dom_models_error_t get_ctx(
    dom_usecases_internal_ota_t*  self,
    app_internal_ota_impl_ctx_t** out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = self->ctx;

    return DOMAIN_MODELS_ERROR_OK;
}

static void on_update_event(
    void*                            cb_ctx,
    const dom_models_update_event_t* event
) {
    const char* tag = BASE_TAG "/on_update_event";

    if (!cb_ctx || !event) {
        return;
    }

    app_internal_ota_impl_ctx_t* ctx = cb_ctx;

    if (event->type == DOM_MODELS_UPDATE_EVENT_PROGRESS) {
        if (event->total_bytes == 0) {
            return;
        }

        int percent = (int)((event->bytes_written * 100) / event->total_bytes);
        if (percent == ctx->last_progress_percent) {
            return;
        }

        ctx->last_progress_percent = percent;
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA update progress: %u/%u bytes (%d%%)", (unsigned int)event->bytes_written, (unsigned int)event->total_bytes, percent);

        return;
    }

    if (event->type != DOM_MODELS_UPDATE_EVENT_COMPLETED) {
        return;
    }

    ctx->last_result = event->result;
    ctx->updating    = false;

    if (event->result != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "OTA update failed: %s (%d)", dom_models_error_str(event->result), (int)event->result);
        return;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "OTA update succeeded, restarting in %u ms", (unsigned int)ctx->cfg.restart_delay_ms);

    dom_models_error_t restart_err = ctx->cfg.system_restart->restart(ctx->cfg.system_restart, ctx->cfg.restart_delay_ms);
    if (restart_err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to request restart after OTA update: %s (%d)", dom_models_error_str(restart_err), (int)restart_err);
    }
}
