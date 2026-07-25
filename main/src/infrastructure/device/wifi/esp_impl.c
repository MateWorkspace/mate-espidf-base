#include "infrastructure/device/wifi/esp_impl.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "domain/contracts/device/wifi.h"
#include "domain/models/error.h"
#include "domain/models/wifi.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "infrastructure/device/wifi/esp_impl_utils.h"

/* Event Handler Function Prototypes */

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data);
static void ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data);
static void on_sta_connected(inf_device_wifi_esp_impl_ctx_t* ctx);
static void on_sta_disconnected(inf_device_wifi_esp_impl_ctx_t* ctx, const wifi_event_sta_disconnected_t* event);
static void on_sta_got_ip(inf_device_wifi_esp_impl_ctx_t* ctx);

/* Helper Function Prototypes */

static void dispatch_event(
    inf_device_wifi_esp_impl_ctx_t* ctx,
    dom_models_wifi_event_type_t    type,
    uint32_t                        driver_status
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

dom_contracts_device_wifi_t* inf_device_wifi_esp_impl_new(const inf_device_wifi_esp_impl_cfg_t* cfg) {
    inf_device_wifi_esp_impl_ctx_t* ctx = (inf_device_wifi_esp_impl_ctx_t*)calloc(1, sizeof(inf_device_wifi_esp_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    if (!cfg) {
        inf_device_wifi_esp_impl_cfg_t default_cfg = INF_DEVICE_WIFI_ESP_IMPL_CFG_DEFAULT();
        memcpy(&ctx->cfg, &default_cfg, sizeof(inf_device_wifi_esp_impl_cfg_t));
    } else {
        memcpy(&ctx->cfg, cfg, sizeof(inf_device_wifi_esp_impl_cfg_t));
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

void inf_device_wifi_esp_impl_delete(dom_contracts_device_wifi_t* self) {
    if (!self) {
        return;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        (void)inf_device_wifi_esp_impl_deinit(self);
        free(ctx);
    }

    dom_contracts_device_wifi_delete(self);
}

dom_models_error_t inf_device_wifi_esp_impl_init(dom_contracts_device_wifi_t* self) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = self->ctx;

    if (ctx->initialized) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    ctx->sta_netif = esp_netif_create_default_wifi_sta();
    if (!ctx->sta_netif) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    if (ctx->cfg.register_event_handler) {
        esp_err_t err = esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            wifi_event_handler,
            ctx,
            &ctx->wifi_event_handler
        );
        if (err != ESP_OK) {
            esp_netif_destroy(ctx->sta_netif);
            ctx->sta_netif = NULL;
            return inf_device_wifi_esp_impl_error_from_esp(err);
        }
        ctx->wifi_event_handler_registered = true;

        err = esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            ip_event_handler,
            ctx,
            &ctx->ip_event_handler
        );
        if (err != ESP_OK) {
            esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ctx->wifi_event_handler);
            ctx->wifi_event_handler_registered = false;
            ctx->wifi_event_handler            = NULL;
            esp_netif_destroy(ctx->sta_netif);
            ctx->sta_netif = NULL;
            return inf_device_wifi_esp_impl_error_from_esp(err);
        }
        ctx->ip_event_handler_registered = true;
    }

    ctx->initialized = true;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_device_wifi_esp_impl_deinit(dom_contracts_device_wifi_t* self) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx    = self->ctx;
    dom_models_error_t              result = DOMAIN_MODELS_ERROR_OK;

    if (ctx->started) {
        esp_err_t err = esp_wifi_stop();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED && result == DOMAIN_MODELS_ERROR_OK) {
            result = inf_device_wifi_esp_impl_error_from_esp(err);
        }
    }

    if (ctx->wifi_event_handler_registered) {
        esp_err_t err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ctx->wifi_event_handler);
        if (err != ESP_OK) {
            if (result == DOMAIN_MODELS_ERROR_OK) {
                result = inf_device_wifi_esp_impl_error_from_esp(err);
            }
        } else {
            ctx->wifi_event_handler_registered = false;
            ctx->wifi_event_handler            = NULL;
        }
    }

    if (ctx->ip_event_handler_registered) {
        esp_err_t err = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ctx->ip_event_handler);
        if (err != ESP_OK) {
            if (result == DOMAIN_MODELS_ERROR_OK) {
                result = inf_device_wifi_esp_impl_error_from_esp(err);
            }
        } else {
            ctx->ip_event_handler_registered = false;
            ctx->ip_event_handler            = NULL;
        }
    }

    if (ctx->sta_netif) {
        esp_wifi_clear_default_wifi_driver_and_handlers(ctx->sta_netif);
        esp_netif_destroy(ctx->sta_netif);
        ctx->sta_netif = NULL;
    }

    ctx->initialized   = false;
    ctx->started       = false;
    ctx->sta_connected = false;

    return result;
}

/* Contract Function Implementations */

static dom_models_error_t start_impl(
    dom_contracts_device_wifi_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = self->ctx;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return inf_device_wifi_esp_impl_error_from_esp(err);
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        return inf_device_wifi_esp_impl_error_from_esp(err);
    }

    ctx->started = true;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t stop_impl(
    dom_contracts_device_wifi_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = self->ctx;

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK) {
        return inf_device_wifi_esp_impl_error_from_esp(err);
    }

    ctx->started       = false;
    ctx->sta_connected = false;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_status_impl(
    dom_contracts_device_wifi_t* self,
    dom_models_wifi_status_t*    out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = self->ctx;

    memset(out, 0, sizeof(dom_models_wifi_status_t));

    out->is_up                 = ctx->started;
    out->sta_connection_status = ctx->sta_connected ? DOM_MODELS_WIFI_STA_STATUS_CONNECTED : DOM_MODELS_WIFI_STA_STATUS_DISCONNECTED;

    if (ctx->sta_connected) {
        wifi_ap_record_t ap_record;
        memset(&ap_record, 0, sizeof(wifi_ap_record_t));
        if (esp_wifi_sta_get_ap_info(&ap_record) == ESP_OK) {
            inf_device_wifi_esp_impl_copy_bytes_to_cstr(out->sta_ssid, sizeof(out->sta_ssid), ap_record.ssid, sizeof(ap_record.ssid));
            out->sta_rssi = ap_record.rssi;
        }
    }

    if (ctx->sta_netif) {
        esp_netif_ip_info_t ip_info;
        memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
        if (esp_netif_get_ip_info(ctx->sta_netif, &ip_info) == ESP_OK) {
            inf_device_wifi_esp_impl_ip4_to_bytes(ip_info.ip, out->sta_ipv4);
            inf_device_wifi_esp_impl_ip4_to_bytes(ip_info.netmask, out->sta_netmask);
            inf_device_wifi_esp_impl_ip4_to_bytes(ip_info.gw, out->sta_gateway);
        }
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t connect_sta_impl(
    dom_contracts_device_wifi_t*                self,
    const dom_models_wifi_sta_connect_config_t* config
) {
    if (!self || !self->ctx || !config) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    if (inf_device_wifi_esp_impl_bounded_strlen(config->ssid, sizeof(config->ssid)) == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = self->ctx;
    if (!ctx->started) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    inf_device_wifi_esp_impl_copy_cstr_to_bytes(wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), config->ssid);
    inf_device_wifi_esp_impl_copy_cstr_to_bytes(wifi_config.sta.password, sizeof(wifi_config.sta.password), config->password);

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        return inf_device_wifi_esp_impl_error_from_esp(err);
    }

    err = esp_wifi_connect();
    if (err != ESP_OK) {
        return inf_device_wifi_esp_impl_error_from_esp(err);
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t disconnect_sta_impl(
    dom_contracts_device_wifi_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        return inf_device_wifi_esp_impl_error_from_esp(err);
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = self->ctx;
    ctx->sta_connected                  = false;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t add_event_callback_impl(
    dom_contracts_device_wifi_t*     self,
    void*                            cb_ctx,
    dom_models_wifi_event_callback_t cb_func
) {
    if (!self || !self->ctx || !cb_func) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = self->ctx;

    for (size_t i = 0; i < ctx->event_cb_cnt; i++) {
        if (ctx->event_cb_funcs[i] == cb_func) {
            return DOMAIN_MODELS_ERROR_OK;
        }
    }

    if (ctx->event_cb_cnt >= INF_DEVICE_WIFI_ESP_IMPL_EVENT_CALLBACK_MAX) {
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
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = self->ctx;

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

/* Event Handler Function Implementations */

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (!arg || base != WIFI_EVENT) {
        return;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = arg;

    switch (id) {
        case WIFI_EVENT_STA_CONNECTED:
            on_sta_connected(ctx);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            on_sta_disconnected(ctx, (const wifi_event_sta_disconnected_t*)data);
            break;
        default:
            break;
    }
}

static void on_sta_connected(inf_device_wifi_esp_impl_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    ctx->sta_connected = true;
    dispatch_event(ctx, DOM_MODELS_WIFI_EVENT_STA_CONNECTED, 0);
}

static void on_sta_disconnected(inf_device_wifi_esp_impl_ctx_t* ctx, const wifi_event_sta_disconnected_t* event) {
    if (!ctx) {
        return;
    }

    ctx->sta_connected = false;
    dispatch_event(ctx, DOM_MODELS_WIFI_EVENT_STA_DISCONNECTED, event ? (uint32_t)event->reason : 0);
}

static void ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (!arg || base != IP_EVENT) {
        return;
    }

    inf_device_wifi_esp_impl_ctx_t* ctx = arg;

    switch (id) {
        case IP_EVENT_STA_GOT_IP:
            (void)data;
            on_sta_got_ip(ctx);
            break;
        default:
            break;
    }
}

static void on_sta_got_ip(inf_device_wifi_esp_impl_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    dispatch_event(ctx, DOM_MODELS_WIFI_EVENT_STA_GOT_IP, 0);
}

/* Helper Function Implementations */

static void dispatch_event(
    inf_device_wifi_esp_impl_ctx_t* ctx,
    dom_models_wifi_event_type_t    type,
    uint32_t                        driver_status
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
