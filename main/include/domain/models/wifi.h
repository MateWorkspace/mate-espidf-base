#ifndef DOMAIN_MODELS_WIFI_H
#define DOMAIN_MODELS_WIFI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DOM_MODELS_WIFI_STA_CREDENTIAL_SSID_KEY "wifi_sta_ssid"
#define DOM_MODELS_WIFI_STA_CREDENTIAL_PASS_KEY "wifi_sta_pass"

typedef enum {
    DOM_MODELS_WIFI_STA_STATUS_DISCONNECTED,
    DOM_MODELS_WIFI_STA_STATUS_CONNECTED
} dom_models_wifi_sta_status_t;

typedef struct {
    bool                         is_up;
    dom_models_wifi_sta_status_t sta_connection_status;
    char                         sta_ssid[32 + 1];
    uint8_t                      sta_ipv4[4];
    uint8_t                      sta_netmask[4];
    uint8_t                      sta_gateway[4];
    int8_t                       sta_rssi;
} dom_models_wifi_status_t;

typedef struct {
    char ssid[32 + 1];
    char password[64 + 1];
} dom_models_wifi_sta_connect_config_t;

typedef enum {
    DOM_MODELS_WIFI_EVENT_STA_CONNECTED = 0,
    DOM_MODELS_WIFI_EVENT_STA_DISCONNECTED,
    DOM_MODELS_WIFI_EVENT_STA_GOT_IP,
} dom_models_wifi_event_type_t;

typedef struct {
    dom_models_wifi_event_type_t type;
    uint32_t                     driver_status;
} dom_models_wifi_event_t;

typedef void (*dom_models_wifi_event_callback_t)(void* cb_ctx, const dom_models_wifi_event_t* event);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_WIFI_H */
