#include "application/internal/messaging_callbacks/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/messaging_callbacks/impl_types.h"
#include "application/internal/messaging_callbacks/impl_utils.h"
#include "domain/models/device_status.h"
#include "domain/models/error.h"
#include "domain/models/system.h"
#include "domain/usecases/internal/messaging_callbacks.h"

#define BASE_TAG "internal_messaging_callbacks"

/* Helper Function Prototypes */

static dom_models_error_t get_ctx(
    dom_usecases_internal_messaging_callbacks_t*  self,
    app_internal_messaging_callbacks_impl_ctx_t** out
);

static dom_models_error_t load_device_id_str(
    app_internal_messaging_callbacks_impl_ctx_t* ctx,
    char*                                        out,
    size_t                                       out_size
);

static void on_log_message(
    void*       cb_ctx,
    const char* msg,
    size_t      msg_len
);

/* Contract Function Prototypes */

static dom_models_error_t publish_registration_impl(
    dom_usecases_internal_messaging_callbacks_t* self
);
static dom_models_error_t publish_online_status_impl(
    dom_usecases_internal_messaging_callbacks_t* self
);
static dom_models_error_t subscribe_defaults_impl(
    dom_usecases_internal_messaging_callbacks_t* self
);
static dom_models_error_t restart_impl(
    dom_usecases_internal_messaging_callbacks_t* self,
    uint32_t                                     delay_ms
);
static dom_models_error_t publish_action_ack_impl(
    dom_usecases_internal_messaging_callbacks_t* self,
    const char*                                  execution_id,
    const char*                                  status,
    const char*                                  message
);

/* Constructor and Destructor */

dom_usecases_internal_messaging_callbacks_t* app_internal_messaging_callbacks_impl_new(const app_internal_messaging_callbacks_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    dom_models_error_t err = app_internal_messaging_callbacks_impl_validate_cfg(cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_messaging_callbacks_impl_ctx_t* ctx = (app_internal_messaging_callbacks_impl_ctx_t*)calloc(1, sizeof(app_internal_messaging_callbacks_impl_ctx_t));
    if (!ctx) {
        cfg->logger->error(cfg->logger, tag, "Failed to allocate messaging callbacks context: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_messaging_callbacks_impl_cfg_t));

    err = ctx->cfg.preloaded_repository->get_device_id_str(ctx->cfg.preloaded_repository, ctx->device_id_str, sizeof(ctx->device_id_str));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load device id: %s (%d)", dom_models_error_str(err), (int)err);
        free(ctx);
        return NULL;
    }

    dom_usecases_internal_messaging_callbacks_t* self = dom_usecases_internal_messaging_callbacks_new(ctx);
    if (!self) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to allocate messaging callbacks usecase: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        free(ctx);
        return NULL;
    }

    self->publish_registration  = publish_registration_impl;
    self->publish_online_status = publish_online_status_impl;
    self->subscribe_defaults    = subscribe_defaults_impl;
    self->restart               = restart_impl;
    self->publish_action_ack    = publish_action_ack_impl;

    ctx->cfg.logger->add_callback(ctx->cfg.logger, ctx, on_log_message);

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks created successfully");

    return self;
}

void app_internal_messaging_callbacks_impl_delete(dom_usecases_internal_messaging_callbacks_t* self) {
    const char* tag = BASE_TAG "/delete";

    if (!self) {
        return;
    }

    app_internal_messaging_callbacks_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->remove_callback(ctx->cfg.logger, on_log_message);
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Messaging callbacks deleted successfully");
        free(ctx);
    }

    dom_usecases_internal_messaging_callbacks_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t publish_registration_impl(
    dom_usecases_internal_messaging_callbacks_t* self
) {
    const char* tag = BASE_TAG "/publish_registration";

    app_internal_messaging_callbacks_impl_ctx_t* ctx = NULL;
    dom_models_error_t                           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    char device_id_str[37];
    err = load_device_id_str(ctx, device_id_str, sizeof(device_id_str));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load device id: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    dom_models_system_project_info_t project_info;
    err = ctx->cfg.system_info->get_project_info(ctx->cfg.system_info, &project_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load project info: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    dom_models_system_chip_info_t chip_info;
    err = ctx->cfg.system_info->get_chip_info(ctx->cfg.system_info, &chip_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load chip info: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    char firmware_name[96];
    err = app_internal_messaging_callbacks_impl_build_firmware_name(&project_info, firmware_name, sizeof(firmware_name));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to build firmware name: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    char device_info[96];
    err = app_internal_messaging_callbacks_impl_build_device_info(&chip_info, device_info, sizeof(device_info));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to build device info: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.def_pub->registration(ctx->cfg.def_pub, device_id_str, device_info, firmware_name);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to publish registration: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Registration published successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t publish_online_status_impl(
    dom_usecases_internal_messaging_callbacks_t* self
) {
    const char* tag = BASE_TAG "/publish_online_status";

    app_internal_messaging_callbacks_impl_ctx_t* ctx = NULL;
    dom_models_error_t                           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    char device_id_str[37];
    err = load_device_id_str(ctx, device_id_str, sizeof(device_id_str));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load device id: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.def_pub->status(ctx->cfg.def_pub, device_id_str, DOM_MODELS_DEVICE_STATUS_ONLINE);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to publish online status: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Online status published successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t subscribe_defaults_impl(
    dom_usecases_internal_messaging_callbacks_t* self
) {
    const char* tag = BASE_TAG "/subscribe_defaults";

    app_internal_messaging_callbacks_impl_ctx_t* ctx = NULL;
    dom_models_error_t                           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    char device_id_str[37];
    err = load_device_id_str(ctx, device_id_str, sizeof(device_id_str));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load device id: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.def_sub->registration_ack(device_id_str, ctx->cfg.def_sub);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to registration_ack: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.def_sub->ota(device_id_str, ctx->cfg.def_sub);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to ota: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.def_sub->action(device_id_str, ctx->cfg.def_sub);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to action: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Default subscriptions completed successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t restart_impl(
    dom_usecases_internal_messaging_callbacks_t* self,
    uint32_t                                     delay_ms
) {
    const char* tag = BASE_TAG "/restart";

    app_internal_messaging_callbacks_impl_ctx_t* ctx = NULL;
    dom_models_error_t                           err = get_ctx(self, &ctx);
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

static dom_models_error_t publish_action_ack_impl(
    dom_usecases_internal_messaging_callbacks_t* self,
    const char*                                  execution_id,
    const char*                                  status,
    const char*                                  message
) {
    const char* tag = BASE_TAG "/publish_action_ack";

    app_internal_messaging_callbacks_impl_ctx_t* ctx = NULL;
    dom_models_error_t                           err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    char device_id_str[37];
    err = load_device_id_str(ctx, device_id_str, sizeof(device_id_str));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load device id: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.def_pub->action_ack(ctx->cfg.def_pub, device_id_str, execution_id, status, message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to publish action ack: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Action ack published successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static dom_models_error_t get_ctx(
    dom_usecases_internal_messaging_callbacks_t*  self,
    app_internal_messaging_callbacks_impl_ctx_t** out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = self->ctx;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t load_device_id_str(
    app_internal_messaging_callbacks_impl_ctx_t* ctx,
    char*                                        out,
    size_t                                       out_size
) {
    if (!ctx || !out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return ctx->cfg.preloaded_repository->get_device_id_str(ctx->cfg.preloaded_repository, out, out_size);
}

static void on_log_message(
    void*       cb_ctx,
    const char* msg,
    size_t      msg_len
) {
    if (!cb_ctx || !msg || msg_len == 0) {
        return;
    }

    app_internal_messaging_callbacks_impl_ctx_t* ctx = cb_ctx;

    /* Never call back into ctx->cfg.logger here: this callback is registered on that
     * same logger instance, and logging from within it would re-enter print_log() ->
     * run_callbacks() -> this function, recursing without end. */
    (void)ctx->cfg.def_pub->log(ctx->cfg.def_pub, ctx->device_id_str, msg, msg_len);
}
