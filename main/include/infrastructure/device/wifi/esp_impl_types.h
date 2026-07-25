#ifndef INFRASTRUCTURE_DEVICE_WIFI_ESP_IMPL_TYPES_H
#define INFRASTRUCTURE_DEVICE_WIFI_ESP_IMPL_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#include "domain/models/wifi.h"
#include "esp_event_base.h"
#include "esp_netif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool register_event_handler;
} inf_device_wifi_esp_impl_cfg_t;

#define INF_DEVICE_WIFI_ESP_IMPL_EVENT_CALLBACK_MAX 4

#define INF_DEVICE_WIFI_ESP_IMPL_CFG_DEFAULT() \
    {                                          \
        .register_event_handler = true,        \
    }

typedef struct {
    inf_device_wifi_esp_impl_cfg_t   cfg;
    esp_netif_t*                     sta_netif;
    esp_event_handler_instance_t     wifi_event_handler;
    esp_event_handler_instance_t     ip_event_handler;
    bool                             initialized;
    bool                             wifi_event_handler_registered;
    bool                             ip_event_handler_registered;
    bool                             started;
    bool                             sta_connected;
    dom_models_wifi_event_callback_t event_cb_funcs[INF_DEVICE_WIFI_ESP_IMPL_EVENT_CALLBACK_MAX];
    void*                            event_cb_ctxs[INF_DEVICE_WIFI_ESP_IMPL_EVENT_CALLBACK_MAX];
    size_t                           event_cb_cnt;
} inf_device_wifi_esp_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_WIFI_ESP_IMPL_TYPES_H */
