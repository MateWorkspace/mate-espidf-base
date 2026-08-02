#include "application/internal/settings/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/settings/impl_types.h"
#include "application/internal/settings/impl_utils.h"
#include "domain/models/error.h"
#include "domain/usecases/internal/settings.h"

#define BASE_TAG "internal_settings"

/* Helper Function Prototypes */

static dom_models_error_t get_ctx(
    dom_usecases_internal_settings_t*  self,
    app_internal_settings_impl_ctx_t** out
);

/* Contract Function Prototypes */

static dom_models_error_t get_snapshot_impl(
    dom_usecases_internal_settings_t*          self,
    dom_usecases_internal_settings_snapshot_t* out
);
static dom_models_error_t set_preloaded_impl(
    dom_usecases_internal_settings_t*                        self,
    const dom_usecases_internal_settings_preloaded_update_t* update,
    bool*                                                    restart_required_out
);
static dom_models_error_t get_restart_required_impl(
    dom_usecases_internal_settings_t* self,
    bool*                             out
);
static dom_models_error_t restart_impl(
    dom_usecases_internal_settings_t* self,
    uint32_t                          delay_ms
);

/* Constructor and Destructor */

dom_usecases_internal_settings_t* app_internal_settings_impl_new(const app_internal_settings_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    dom_models_error_t err = app_internal_settings_impl_validate_cfg(cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_settings_impl_ctx_t* ctx = (app_internal_settings_impl_ctx_t*)calloc(1, sizeof(app_internal_settings_impl_ctx_t));
    if (!ctx) {
        cfg->logger->error(cfg->logger, tag, "Failed to allocate Settings context: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_settings_impl_cfg_t));

    dom_usecases_internal_settings_t* self = dom_usecases_internal_settings_new(ctx);
    if (!self) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to allocate Settings usecase: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        free(ctx);
        return NULL;
    }

    self->get_snapshot         = get_snapshot_impl;
    self->set_preloaded        = set_preloaded_impl;
    self->get_restart_required = get_restart_required_impl;
    self->restart              = restart_impl;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Settings created successfully");

    return self;
}

void app_internal_settings_impl_delete(dom_usecases_internal_settings_t* self) {
    const char* tag = BASE_TAG "/delete";

    if (!self) {
        return;
    }

    app_internal_settings_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Settings deleted successfully");
        free(ctx);
    }

    dom_usecases_internal_settings_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t get_snapshot_impl(
    dom_usecases_internal_settings_t*          self,
    dom_usecases_internal_settings_snapshot_t* out
) {
    const char* tag = BASE_TAG "/get_snapshot";

    app_internal_settings_impl_ctx_t* ctx = NULL;
    dom_models_error_t                err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing settings snapshot output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = app_internal_settings_impl_load_snapshot(ctx, out);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load settings snapshot: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Settings snapshot retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t set_preloaded_impl(
    dom_usecases_internal_settings_t*                        self,
    const dom_usecases_internal_settings_preloaded_update_t* update,
    bool*                                                    restart_required_out
) {
    const char* tag = BASE_TAG "/set_preloaded";

    app_internal_settings_impl_ctx_t* ctx = NULL;
    dom_models_error_t                err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!app_internal_settings_impl_has_preloaded_update(update)) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing preloaded update values: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    if (update->mqtt_proto_set) {
        err = ctx->cfg.preloaded_repository->set_mqtt_proto(ctx->cfg.preloaded_repository, update->mqtt_proto);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to set MQTT protocol: %s (%d)", dom_models_error_str(err), (int)err);
            return err;
        }
        ctx->restart_required = true;
    }

    if (update->mqtt_host_set) {
        err = ctx->cfg.preloaded_repository->set_mqtt_host(ctx->cfg.preloaded_repository, update->mqtt_host);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to set MQTT host: %s (%d)", dom_models_error_str(err), (int)err);
            return err;
        }
        ctx->restart_required = true;
    }

    if (update->mqtt_port_set) {
        err = ctx->cfg.preloaded_repository->set_mqtt_port(ctx->cfg.preloaded_repository, update->mqtt_port);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to set MQTT port: %s (%d)", dom_models_error_str(err), (int)err);
            return err;
        }
        ctx->restart_required = true;
    }

    if (update->mqtt_user_set) {
        err = ctx->cfg.preloaded_repository->set_mqtt_user(ctx->cfg.preloaded_repository, update->mqtt_user);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to set MQTT user: %s (%d)", dom_models_error_str(err), (int)err);
            return err;
        }
        ctx->restart_required = true;
    }

    if (update->mqtt_pass_set) {
        err = ctx->cfg.preloaded_repository->set_mqtt_pass(ctx->cfg.preloaded_repository, update->mqtt_pass);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to set MQTT password: %s (%d)", dom_models_error_str(err), (int)err);
            return err;
        }
        ctx->restart_required = true;
    }

    if (update->system_restart_after_ms_set) {
        err = ctx->cfg.preloaded_repository->set_system_restart_after_ms(ctx->cfg.preloaded_repository, update->system_restart_after_ms);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to set system restart after ms: %s (%d)", dom_models_error_str(err), (int)err);
            return err;
        }
        ctx->restart_required = true;
    }

    if (update->wifi_try_init_set) {
        err = ctx->cfg.preloaded_repository->set_wifi_sta_try_connect_on_init(ctx->cfg.preloaded_repository, update->wifi_try_init);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to set WiFi try-connect-on-init: %s (%d)", dom_models_error_str(err), (int)err);
            return err;
        }
        ctx->restart_required = true;
    }

    if (restart_required_out) {
        *restart_required_out = ctx->restart_required;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Preloaded settings updated successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_restart_required_impl(
    dom_usecases_internal_settings_t* self,
    bool*                             out
) {
    const char* tag = BASE_TAG "/get_restart_required";

    app_internal_settings_impl_ctx_t* ctx = NULL;
    dom_models_error_t                err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing restart required output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    *out = ctx->restart_required;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Restart required state retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t restart_impl(
    dom_usecases_internal_settings_t* self,
    uint32_t                          delay_ms
) {
    const char* tag = BASE_TAG "/restart";

    app_internal_settings_impl_ctx_t* ctx = NULL;
    dom_models_error_t                err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.system_restart->restart(ctx->cfg.system_restart, delay_ms);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to restart system: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "System restart requested successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static dom_models_error_t get_ctx(
    dom_usecases_internal_settings_t*  self,
    app_internal_settings_impl_ctx_t** out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = self->ctx;

    return DOMAIN_MODELS_ERROR_OK;
}
