#include "application/internal/system_info/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/system_info/impl_types.h"
#include "application/internal/system_info/impl_utils.h"
#include "domain/models/error.h"
#include "domain/usecases/internal/system_info.h"

#define BASE_TAG "internal_system_info"

/* Helper Function Prototypes */

static dom_models_error_t get_ctx(
    dom_usecases_internal_system_info_t*  self,
    app_internal_system_info_impl_ctx_t** out
);

/* Contract Function Prototypes */

static dom_models_error_t get_project_info_impl(
    dom_usecases_internal_system_info_t* self,
    dom_models_system_project_info_t*    out
);
static dom_models_error_t get_chip_info_impl(
    dom_usecases_internal_system_info_t* self,
    dom_models_system_chip_info_t*       out
);
static dom_models_error_t get_preloaded_schema_impl(
    dom_usecases_internal_system_info_t*        self,
    const dom_models_preloaded_schema_entry_t** out,
    size_t*                                     out_count
);

/* Constructor and Destructor */

dom_usecases_internal_system_info_t* app_internal_system_info_impl_new(const app_internal_system_info_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    dom_models_error_t err = app_internal_system_info_impl_validate_cfg(cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_system_info_impl_ctx_t* ctx = (app_internal_system_info_impl_ctx_t*)calloc(1, sizeof(app_internal_system_info_impl_ctx_t));
    if (!ctx) {
        cfg->logger->error(cfg->logger, tag, "Failed to allocate System Info context: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_system_info_impl_cfg_t));

    dom_usecases_internal_system_info_t* self = dom_usecases_internal_system_info_new(ctx);
    if (!self) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to allocate System Info usecase: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        free(ctx);
        return NULL;
    }

    self->get_project_info     = get_project_info_impl;
    self->get_chip_info        = get_chip_info_impl;
    self->get_preloaded_schema = get_preloaded_schema_impl;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "System Info created successfully");

    return self;
}

void app_internal_system_info_impl_delete(dom_usecases_internal_system_info_t* self) {
    const char* tag = BASE_TAG "/delete";

    if (!self) {
        return;
    }

    app_internal_system_info_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "System Info deleted successfully");
        free(ctx);
    }

    dom_usecases_internal_system_info_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t get_project_info_impl(
    dom_usecases_internal_system_info_t* self,
    dom_models_system_project_info_t*    out
) {
    const char* tag = BASE_TAG "/get_project_info";

    app_internal_system_info_impl_ctx_t* ctx = NULL;
    dom_models_error_t                   err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing project info output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.system_info->get_project_info(ctx->cfg.system_info, out);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load project info: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Project info retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_chip_info_impl(
    dom_usecases_internal_system_info_t* self,
    dom_models_system_chip_info_t*       out
) {
    const char* tag = BASE_TAG "/get_chip_info";

    app_internal_system_info_impl_ctx_t* ctx = NULL;
    dom_models_error_t                   err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing chip info output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.system_info->get_chip_info(ctx->cfg.system_info, out);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load chip info: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Chip info retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_preloaded_schema_impl(
    dom_usecases_internal_system_info_t*        self,
    const dom_models_preloaded_schema_entry_t** out,
    size_t*                                     out_count
) {
    const char* tag = BASE_TAG "/get_preloaded_schema";

    app_internal_system_info_impl_ctx_t* ctx = NULL;
    dom_models_error_t                   err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out || !out_count) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing preloaded schema output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    *out       = dom_models_preloaded_schema;
    *out_count = DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Preloaded config schema retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static dom_models_error_t get_ctx(
    dom_usecases_internal_system_info_t*  self,
    app_internal_system_info_impl_ctx_t** out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = self->ctx;

    return DOMAIN_MODELS_ERROR_OK;
}
