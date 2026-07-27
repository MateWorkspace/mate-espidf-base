#include "application/internal/wifi_manager/impl.h"

#include <stdlib.h>
#include <string.h>

#include "application/internal/wifi_manager/impl_types.h"
#include "application/internal/wifi_manager/impl_utils.h"
#include "domain/models/error.h"
#include "domain/models/wifi.h"
#include "domain/usecases/internal/wifi_manager.h"

#define BASE_TAG "internal_wifi_manager"

/* Helper Function Prototypes */

static dom_models_error_t get_ctx(
    dom_usecases_internal_wifi_manager_t*  self,
    app_internal_wifi_manager_impl_ctx_t** out
);

/* Contract Function Prototypes */

static dom_models_error_t start_impl(
    dom_usecases_internal_wifi_manager_t* self
);
static dom_models_error_t stop_impl(
    dom_usecases_internal_wifi_manager_t* self
);
static dom_models_error_t get_status_impl(
    dom_usecases_internal_wifi_manager_t*        self,
    dom_usecases_internal_wifi_manager_status_t* out
);
static dom_models_error_t connect_impl(
    dom_usecases_internal_wifi_manager_t*       self,
    const dom_models_wifi_sta_connect_config_t* credential
);
static dom_models_error_t connect_stored_impl(
    dom_usecases_internal_wifi_manager_t* self
);
static dom_models_error_t disconnect_impl(
    dom_usecases_internal_wifi_manager_t* self
);
static dom_models_error_t get_stored_credential_impl(
    dom_usecases_internal_wifi_manager_t*            self,
    dom_usecases_internal_wifi_manager_stored_sta_t* out
);
static dom_models_error_t forget_stored_credential_impl(
    dom_usecases_internal_wifi_manager_t* self
);
static dom_models_error_t get_try_connect_on_init_impl(
    dom_usecases_internal_wifi_manager_t* self,
    bool*                                 out
);
static dom_models_error_t set_try_connect_on_init_impl(
    dom_usecases_internal_wifi_manager_t* self,
    bool                                  enabled
);
static dom_models_error_t need_reconnect_impl(
    dom_usecases_internal_wifi_manager_t* self,
    bool*                                 out
);
static dom_models_error_t try_reconnect_impl(
    dom_usecases_internal_wifi_manager_t* self,
    bool*                                 attempted
);
static dom_models_error_t add_status_callback_impl(
    dom_usecases_internal_wifi_manager_t* self,
    void*                                 cb_ctx,
    dom_models_wifi_event_callback_t      cb_func
);
static dom_models_error_t remove_status_callback_impl(
    dom_usecases_internal_wifi_manager_t* self,
    dom_models_wifi_event_callback_t      cb_func
);

/* Event Handler Prototype */

static void on_wifi_event(void* cb_ctx, const dom_models_wifi_event_t* event);

/* Constructor and Destructor */

dom_usecases_internal_wifi_manager_t* app_internal_wifi_manager_impl_new(const app_internal_wifi_manager_impl_cfg_t* cfg) {
    const char* tag = BASE_TAG "/new";

    dom_models_error_t err = app_internal_wifi_manager_impl_validate_cfg(cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    app_internal_wifi_manager_impl_ctx_t* ctx = (app_internal_wifi_manager_impl_ctx_t*)calloc(1, sizeof(app_internal_wifi_manager_impl_ctx_t));
    if (!ctx) {
        cfg->logger->error(cfg->logger, tag, "Failed to allocate WiFi manager context: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(app_internal_wifi_manager_impl_cfg_t));
    ctx->connect_attempted = false;

    dom_usecases_internal_wifi_manager_t* self = dom_usecases_internal_wifi_manager_new(ctx);
    if (!self) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to allocate WiFi manager usecase: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        free(ctx);
        return NULL;
    }

    self->start                    = start_impl;
    self->stop                     = stop_impl;
    self->get_status               = get_status_impl;
    self->connect                  = connect_impl;
    self->connect_stored           = connect_stored_impl;
    self->disconnect               = disconnect_impl;
    self->get_stored_credential    = get_stored_credential_impl;
    self->forget_stored_credential = forget_stored_credential_impl;
    self->get_try_connect_on_init  = get_try_connect_on_init_impl;
    self->set_try_connect_on_init  = set_try_connect_on_init_impl;
    self->need_reconnect           = need_reconnect_impl;
    self->try_reconnect            = try_reconnect_impl;
    self->add_status_callback      = add_status_callback_impl;
    self->remove_status_callback   = remove_status_callback_impl;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "WiFi manager created successfully");

    return self;
}

void app_internal_wifi_manager_impl_delete(dom_usecases_internal_wifi_manager_t* self) {
    const char* tag = BASE_TAG "/delete";

    if (!self) {
        return;
    }

    app_internal_wifi_manager_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "WiFi manager deleted successfully");
        free(ctx);
    }

    dom_usecases_internal_wifi_manager_delete(self);
}

dom_models_error_t app_internal_wifi_manager_impl_init(dom_usecases_internal_wifi_manager_t* self) {
    const char* tag = BASE_TAG "/init";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (ctx->event_subscribed) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    err = ctx->cfg.wifi->add_event_callback(ctx->cfg.wifi, ctx, on_wifi_event);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to WiFi events: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->event_subscribed = true;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "WiFi manager initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void app_internal_wifi_manager_impl_deinit(dom_usecases_internal_wifi_manager_t* self) {
    const char* tag = BASE_TAG "/deinit";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    if (get_ctx(self, &ctx) != DOMAIN_MODELS_ERROR_OK || !ctx->event_subscribed) {
        return;
    }

    ctx->cfg.wifi->remove_event_callback(ctx->cfg.wifi, on_wifi_event);
    ctx->event_subscribed = false;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "WiFi manager deinitialized successfully");
}

/* Contract Function Implementations */

static dom_models_error_t start_impl(
    dom_usecases_internal_wifi_manager_t* self
) {
    const char* tag = BASE_TAG "/start";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.wifi->start(ctx->cfg.wifi);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to start WiFi: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "WiFi started successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t stop_impl(
    dom_usecases_internal_wifi_manager_t* self
) {
    const char* tag = BASE_TAG "/stop";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.wifi->stop(ctx->cfg.wifi);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to stop WiFi: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->connect_attempted = false;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "WiFi stopped successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_status_impl(
    dom_usecases_internal_wifi_manager_t*        self,
    dom_usecases_internal_wifi_manager_status_t* out
) {
    const char* tag = BASE_TAG "/get_status";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing WiFi manager status output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    memset(out, 0, sizeof(dom_usecases_internal_wifi_manager_status_t));

    err = ctx->cfg.wifi->get_status(ctx->cfg.wifi, &out->wifi);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to get WiFi status: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = app_internal_wifi_manager_impl_load_stored_sta(ctx, &out->stored);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load stored STA credential view: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.preloaded_repository->get_wifi_sta_try_connect_on_init(ctx->cfg.preloaded_repository, &out->try_connect_on_init_enabled);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to get try-connect-on-init flag: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    out->connect_attempted = ctx->connect_attempted;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "WiFi manager status retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t connect_impl(
    dom_usecases_internal_wifi_manager_t*       self,
    const dom_models_wifi_sta_connect_config_t* credential
) {
    const char* tag = BASE_TAG "/connect";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = app_internal_wifi_manager_impl_validate_credential(credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Invalid STA credential: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->connect_attempted = true;

    err = ctx->cfg.wifi->connect_sta(ctx->cfg.wifi, credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to connect STA: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.wifi_repository->set_sta_credential(ctx->cfg.wifi_repository, credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to store STA credential: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "STA connection request accepted and credential stored successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t connect_stored_impl(
    dom_usecases_internal_wifi_manager_t* self
) {
    const char* tag = BASE_TAG "/connect_stored";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    dom_models_wifi_sta_connect_config_t credential;
    err = ctx->cfg.wifi_repository->get_sta_credential(ctx->cfg.wifi_repository, &credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load stored STA credential: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = app_internal_wifi_manager_impl_validate_credential(&credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Stored STA credential is invalid: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->connect_attempted = true;

    err = ctx->cfg.wifi->connect_sta(ctx->cfg.wifi, &credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to connect using stored STA credential: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Stored STA connection request accepted successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t disconnect_impl(
    dom_usecases_internal_wifi_manager_t* self
) {
    const char* tag = BASE_TAG "/disconnect";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.wifi->disconnect_sta(ctx->cfg.wifi);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to disconnect STA: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->connect_attempted = false;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "STA disconnected successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_stored_credential_impl(
    dom_usecases_internal_wifi_manager_t*            self,
    dom_usecases_internal_wifi_manager_stored_sta_t* out
) {
    const char* tag = BASE_TAG "/get_stored_credential";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing stored STA output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = app_internal_wifi_manager_impl_load_stored_sta(ctx, out);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load stored STA credential view: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Stored STA credential view retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t forget_stored_credential_impl(
    dom_usecases_internal_wifi_manager_t* self
) {
    const char* tag = BASE_TAG "/forget_stored_credential";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.wifi_repository->clear_sta_credential(ctx->cfg.wifi_repository);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to forget stored STA credential: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Stored STA credential forgotten successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_try_connect_on_init_impl(
    dom_usecases_internal_wifi_manager_t* self,
    bool*                                 out
) {
    const char* tag = BASE_TAG "/get_try_connect_on_init";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing try-connect-on-init output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = ctx->cfg.preloaded_repository->get_wifi_sta_try_connect_on_init(ctx->cfg.preloaded_repository, out);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to get try-connect-on-init flag: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Try-connect-on-init flag retrieved successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t set_try_connect_on_init_impl(
    dom_usecases_internal_wifi_manager_t* self,
    bool                                  enabled
) {
    const char* tag = BASE_TAG "/set_try_connect_on_init";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.preloaded_repository->set_wifi_sta_try_connect_on_init(ctx->cfg.preloaded_repository, enabled);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to set try-connect-on-init flag: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Try-connect-on-init flag set successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t need_reconnect_impl(
    dom_usecases_internal_wifi_manager_t* self,
    bool*                                 out
) {
    const char* tag = BASE_TAG "/need_reconnect";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!out) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing reconnect output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    *out = false;

    bool try_connect_on_init = false;
    err                      = ctx->cfg.preloaded_repository->get_wifi_sta_try_connect_on_init(ctx->cfg.preloaded_repository, &try_connect_on_init);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to get try-connect-on-init flag: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    if (!ctx->connect_attempted && !try_connect_on_init) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Reconnect is not needed because no connection trial has happened yet and try-connect-on-init is disabled");
        return DOMAIN_MODELS_ERROR_OK;
    }

    dom_models_wifi_status_t status;
    err = ctx->cfg.wifi->get_status(ctx->cfg.wifi, &status);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to get WiFi status for reconnect decision: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    if (status.sta_connection_status == DOM_MODELS_WIFI_STA_STATUS_CONNECTED) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Reconnect is not needed because STA is connected");
        return DOMAIN_MODELS_ERROR_OK;
    }

    dom_models_wifi_sta_connect_config_t credential;
    err = ctx->cfg.wifi_repository->get_sta_credential(ctx->cfg.wifi_repository, &credential);
    if (err == DOMAIN_MODELS_ERROR_NOT_FOUND) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Reconnect is not needed because no stored credential is available");
        return DOMAIN_MODELS_ERROR_OK;
    }
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load stored credential for reconnect decision: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    if (app_internal_wifi_manager_impl_validate_credential(&credential) != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Reconnect is not needed because stored credential is invalid");
        return DOMAIN_MODELS_ERROR_OK;
    }

    *out = true;

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "Reconnect is needed");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t try_reconnect_impl(
    dom_usecases_internal_wifi_manager_t* self,
    bool*                                 attempted
) {
    const char* tag = BASE_TAG "/try_reconnect";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!attempted) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing reconnect attempted output: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    *attempted = false;

    bool needed = false;
    err         = need_reconnect_impl(self, &needed);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to evaluate reconnect need: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    if (!needed) {
        ctx->cfg.logger->info(ctx->cfg.logger, tag, "Reconnect was not attempted because it is not needed");
        return DOMAIN_MODELS_ERROR_OK;
    }

    dom_models_wifi_sta_connect_config_t credential;
    err = ctx->cfg.wifi_repository->get_sta_credential(ctx->cfg.wifi_repository, &credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to load stored credential for reconnect: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->connect_attempted = true;
    *attempted             = true;

    err = ctx->cfg.wifi->connect_sta(ctx->cfg.wifi, &credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to reconnect using stored STA credential: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->cfg.logger->info(ctx->cfg.logger, tag, "STA reconnect request accepted successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t add_status_callback_impl(
    dom_usecases_internal_wifi_manager_t* self,
    void*                                 cb_ctx,
    dom_models_wifi_event_callback_t      cb_func
) {
    const char* tag = BASE_TAG "/add_status_callback";

    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!cb_func) {
        err = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Missing status callback function: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    if (ctx->status_cb_idx >= APPLICATION_INTERNAL_WIFI_MANAGER_IMPL_STATUS_CB_MAX_CNT) {
        err = DOMAIN_MODELS_ERROR_BAD_STATE;
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "No room left for another status callback: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    ctx->status_cb_funcs[ctx->status_cb_idx] = cb_func;
    ctx->status_cb_ctxs[ctx->status_cb_idx]  = cb_ctx;
    ctx->status_cb_idx += 1;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t remove_status_callback_impl(
    dom_usecases_internal_wifi_manager_t* self,
    dom_models_wifi_event_callback_t      cb_func
) {
    app_internal_wifi_manager_impl_ctx_t* ctx = NULL;
    dom_models_error_t                    err = get_ctx(self, &ctx);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    if (!cb_func || ctx->status_cb_idx == 0) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    for (unsigned int i = 0; i < ctx->status_cb_idx; i++) {
        if (ctx->status_cb_funcs[i] != cb_func) {
            continue;
        }

        unsigned int last_idx = ctx->status_cb_idx - 1;

        ctx->status_cb_funcs[i] = NULL;
        ctx->status_cb_ctxs[i]  = NULL;

        if (i != last_idx) {
            ctx->status_cb_funcs[i] = ctx->status_cb_funcs[last_idx];
            ctx->status_cb_ctxs[i]  = ctx->status_cb_ctxs[last_idx];

            ctx->status_cb_funcs[last_idx] = NULL;
            ctx->status_cb_ctxs[last_idx]  = NULL;
        }

        ctx->status_cb_idx -= 1;
        return DOMAIN_MODELS_ERROR_OK;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

/* Event Handler Implementation */

static void on_wifi_event(void* cb_ctx, const dom_models_wifi_event_t* event) {
    if (!cb_ctx || !event) {
        return;
    }

    app_internal_wifi_manager_impl_ctx_t* ctx = cb_ctx;
    const char*                           tag = BASE_TAG "/on_wifi_event";

    /* Always-on internal observability: log connect/disconnect regardless
       of whether anything is externally subscribed. */
    switch (event->type) {
        case DOM_MODELS_WIFI_EVENT_STA_GOT_IP: {
            dom_models_wifi_status_t status;
            if (ctx->cfg.wifi->get_status(ctx->cfg.wifi, &status) == DOMAIN_MODELS_ERROR_OK) {
                ctx->cfg.logger->info(
                    ctx->cfg.logger,
                    tag,
                    "WiFi connected, IP: %u.%u.%u.%u",
                    status.sta_ipv4[0],
                    status.sta_ipv4[1],
                    status.sta_ipv4[2],
                    status.sta_ipv4[3]
                );
            } else {
                ctx->cfg.logger->info(ctx->cfg.logger, tag, "WiFi connected");
            }
            break;
        }
        case DOM_MODELS_WIFI_EVENT_STA_DISCONNECTED:
            ctx->cfg.logger->info(ctx->cfg.logger, tag, "WiFi disconnected");
            break;
        case DOM_MODELS_WIFI_EVENT_STA_CONNECTED:
        default:
            /* Link layer only, no IP yet - skip to avoid a noisy duplicate
               line right before STA_GOT_IP. */
            break;
    }

    /* Fan out to external subscribers (e.g. the BLE WiFi handler). */
    for (unsigned int i = 0; i < ctx->status_cb_idx; i++) {
        if (ctx->status_cb_funcs[i]) {
            ctx->status_cb_funcs[i](ctx->status_cb_ctxs[i], event);
        }
    }
}

/* Helper Function Implementations */

static dom_models_error_t get_ctx(
    dom_usecases_internal_wifi_manager_t*  self,
    app_internal_wifi_manager_impl_ctx_t** out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = self->ctx;

    return DOMAIN_MODELS_ERROR_OK;
}
