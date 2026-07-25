#ifndef INFRASTRUCTURE_DEVICE_WIFI_STUB_IMPL_TYPES_H
#define INFRASTRUCTURE_DEVICE_WIFI_STUB_IMPL_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/models/wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* default_ssid;
} inf_device_wifi_stub_impl_cfg_t;

#define INF_DEVICE_WIFI_STUB_IMPL_EVENT_CALLBACK_MAX 4

#define INF_DEVICE_WIFI_STUB_IMPL_CFG_DEFAULT() \
    {                                           \
        .default_ssid = "mate-stub",            \
    }

typedef struct {
    inf_device_wifi_stub_impl_cfg_t  cfg;
    bool                             started;
    bool                             connected;
    char                             connected_ssid[32 + 1];
    int8_t                           connected_rssi;
    dom_models_wifi_event_callback_t event_cb_funcs[INF_DEVICE_WIFI_STUB_IMPL_EVENT_CALLBACK_MAX];
    void*                            event_cb_ctxs[INF_DEVICE_WIFI_STUB_IMPL_EVENT_CALLBACK_MAX];
    size_t                           event_cb_cnt;
} inf_device_wifi_stub_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_WIFI_STUB_IMPL_TYPES_H */
