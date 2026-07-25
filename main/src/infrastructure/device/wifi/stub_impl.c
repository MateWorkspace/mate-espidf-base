#include "infrastructure/device/wifi/stub_impl.h"

#include <stdlib.h>
#include <string.h>

#include "domain/contracts/device/wifi.h"
#include "domain/models/error.h"
#include "domain/models/wifi.h"
#include "infrastructure/device/wifi/stub_impl_utils.h"

/* Helper Function Prototypes */

static void dispatch_event(
    inf_device_wifi_stub_impl_ctx_t* ctx,
    dom_models_wifi_event_type_t     type,
    uint32_t                         driver_status
);

/* Contract Function Prototypes */

static dom_models_error_t start_impl(
    dom_contracts_device_wifi_t* self
);
static dom_models_error_t stop_impl(
    dom_contracts_device_wifi_t* self
);
static dom_models_error_t get_status_impl(
    dom_contracts_device_wifi_t* self,
    dom_models_wifi_status_t*    out
);
static dom_models_error_t connect_sta_impl(
    dom_contracts_device_wifi_t*                self,
    const dom_models_wifi_sta_connect_config_t* config
);
static dom_models_error_t disconnect_sta_impl(
    dom_contracts_device_wifi_t* self
);
static dom_models_error_t add_event_callback_impl(
    dom_contracts_device_wifi_t*     self,
    void*                            cb_ctx,
    dom_models_wifi_event_callback_t cb_func
);
static dom_models_error_t remove_event_callback_impl(
    dom_contracts_device_wifi_t*     self,
    dom_models_wifi_event_callback_t cb_func
);

/* Constructor and Destructor */

dom_contracts_device_wifi_t* inf_device_wifi_stub_impl_new(const inf_device_wifi_stub_impl_cfg_t* cfg) {
    inf_device_wifi_stub_impl_ctx_t* ctx = (inf_device_wifi_stub_impl_ctx_t*)calloc(1, sizeof(inf_device_wifi_stub_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    if (!cfg) {
        inf_device_wifi_stub_impl_cfg_t default_cfg = INF_DEVICE_WIFI_STUB_IMPL_CFG_DEFAULT();
        memcpy(&ctx->cfg, &default_cfg, sizeof(inf_device_wifi_stub_impl_cfg_t));
    } else {
        memcpy(&ctx->cfg, cfg, sizeof(inf_device_wifi_stub_impl_cfg_t));
    }

    dom_contracts_device_wifi_t* self = dom_contracts_device_wifi_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->start                 = start_impl;
    self->stop                  = stop_impl;
    self->get_status            = get_status_impl;
    self->connect_sta           = connect_sta_impl;
    self->disconnect_sta        = disconnect_sta_impl;
    self->add_event_callback    = add_event_callback_impl;
    self->remove_event_callback = remove_event_callback_impl;

    return self;
}

void inf_device_wifi_stub_impl_delete(dom_contracts_device_wifi_t* self) {
    if (!self) {
        return;
    }

    inf_device_wifi_stub_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        free(ctx);
    }

    dom_contracts_device_wifi_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t start_impl(
    dom_contracts_device_wifi_t* self
) {
    if (!self || !self->ctx) {
        return inf_device_wifi_stub_impl_bad_argument_error();
    }

    inf_device_wifi_stub_impl_ctx_t* ctx = self->ctx;
    ctx->started                         = true;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t stop_impl(
    dom_contracts_device_wifi_t* self
) {
    if (!self || !self->ctx) {
        return inf_device_wifi_stub_impl_bad_argument_error();
    }

    inf_device_wifi_stub_impl_ctx_t* ctx           = self->ctx;
    bool                             was_connected = ctx->connected;

    ctx->started           = false;
    ctx->connected         = false;
    ctx->connected_ssid[0] = '\0';
    ctx->connected_rssi    = 0;

    if (was_connected) {
        dispatch_event(ctx, DOM_MODELS_WIFI_EVENT_STA_DISCONNECTED, 0);
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_status_impl(
    dom_contracts_device_wifi_t* self,
    dom_models_wifi_status_t*    out
) {
    if (!self || !self->ctx || !out) {
        return inf_device_wifi_stub_impl_bad_argument_error();
    }

    inf_device_wifi_stub_impl_ctx_t* ctx = self->ctx;

    memset(out, 0, sizeof(dom_models_wifi_status_t));

    out->is_up                 = ctx->started;
    out->sta_connection_status = ctx->connected ? DOM_MODELS_WIFI_STA_STATUS_CONNECTED : DOM_MODELS_WIFI_STA_STATUS_DISCONNECTED;

    if (ctx->connected) {
        inf_device_wifi_stub_impl_copy_cstr(out->sta_ssid, sizeof(out->sta_ssid), ctx->connected_ssid);
        out->sta_rssi = ctx->connected_rssi;
        inf_device_wifi_stub_impl_fill_default_ipv4(out->sta_ipv4, out->sta_netmask, out->sta_gateway);
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t connect_sta_impl(
    dom_contracts_device_wifi_t*                self,
    const dom_models_wifi_sta_connect_config_t* config
) {
    if (!self || !self->ctx || !config) {
        return inf_device_wifi_stub_impl_bad_argument_error();
    }

    if (inf_device_wifi_stub_impl_bounded_strlen(config->ssid, sizeof(config->ssid)) == 0) {
        return inf_device_wifi_stub_impl_bad_argument_error();
    }

    inf_device_wifi_stub_impl_ctx_t* ctx = self->ctx;
    if (!ctx->started) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    ctx->connected      = true;
    ctx->connected_rssi = -40;
    inf_device_wifi_stub_impl_copy_cstr(ctx->connected_ssid, sizeof(ctx->connected_ssid), config->ssid);

    dispatch_event(ctx, DOM_MODELS_WIFI_EVENT_STA_CONNECTED, 0);
    dispatch_event(ctx, DOM_MODELS_WIFI_EVENT_STA_GOT_IP, 0);

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t disconnect_sta_impl(
    dom_contracts_device_wifi_t* self
) {
    if (!self || !self->ctx) {
        return inf_device_wifi_stub_impl_bad_argument_error();
    }

    inf_device_wifi_stub_impl_ctx_t* ctx = self->ctx;
    ctx->connected                       = false;
    ctx->connected_ssid[0]               = '\0';
    ctx->connected_rssi                  = 0;

    dispatch_event(ctx, DOM_MODELS_WIFI_EVENT_STA_DISCONNECTED, 0);

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t add_event_callback_impl(
    dom_contracts_device_wifi_t*     self,
    void*                            cb_ctx,
    dom_models_wifi_event_callback_t cb_func
) {
    if (!self || !self->ctx || !cb_func) {
        return inf_device_wifi_stub_impl_bad_argument_error();
    }

    inf_device_wifi_stub_impl_ctx_t* ctx = self->ctx;

    for (size_t i = 0; i < ctx->event_cb_cnt; i++) {
        if (ctx->event_cb_funcs[i] == cb_func) {
            return DOMAIN_MODELS_ERROR_OK;
        }
    }

    if (ctx->event_cb_cnt >= INF_DEVICE_WIFI_STUB_IMPL_EVENT_CALLBACK_MAX) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    ctx->event_cb_funcs[ctx->event_cb_cnt] = cb_func;
    ctx->event_cb_ctxs[ctx->event_cb_cnt]  = cb_ctx;
    ctx->event_cb_cnt += 1;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t remove_event_callback_impl(
    dom_contracts_device_wifi_t*     self,
    dom_models_wifi_event_callback_t cb_func
) {
    if (!self || !self->ctx || !cb_func) {
        return inf_device_wifi_stub_impl_bad_argument_error();
    }

    inf_device_wifi_stub_impl_ctx_t* ctx = self->ctx;

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
    inf_device_wifi_stub_impl_ctx_t* ctx,
    dom_models_wifi_event_type_t     type,
    uint32_t                         driver_status
) {
    if (!ctx) {
        return;
    }

    dom_models_wifi_event_t event = {
        .type          = type,
        .driver_status = driver_status,
    };

    size_t cb_cnt = ctx->event_cb_cnt;
    for (size_t i = 0; i < cb_cnt; i++) {
        if (!ctx->event_cb_funcs[i]) {
            continue;
        }

        ctx->event_cb_funcs[i](ctx->event_cb_ctxs[i], &event);
    }
}
