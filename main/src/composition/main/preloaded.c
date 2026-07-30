#include "composition/main/preloaded.h"  // IWYU pragma: keep

#include <stdio.h>   // IWYU pragma: keep
#include <stdlib.h>  // IWYU pragma: keep
#include <string.h>  // IWYU pragma: keep

#include "domain/models/error.h"      // IWYU pragma: keep
#include "domain/models/preloaded.h"  // IWYU pragma: keep
#include "domain/models/wifi.h"       // IWYU pragma: keep
#include "esp_err.h"                  // IWYU pragma: keep
#include "esp_mac.h"                  // IWYU pragma: keep
#include "nvs.h"                      // IWYU pragma: keep

/* Default Values */

#define DEFAULT_MQTT_PROTO                   "mqtt"
#define DEFAULT_MQTT_HOST                    "192.168.1.1"
#define DEFAULT_MQTT_PORT                    "1883"
#define DEFAULT_MQTT_USER                    ""
#define DEFAULT_MQTT_PASS                    ""
#define DEFAULT_SYSTEM_RESTART_AFTER_MS      0xFFFFFFFF
#define DEFAULT_WIFI_STA_TRY_CONNECT_ON_INIT false

#define DEVICE_ID_STR_LEN 12

/* Shared Variable */

dom_models_preloaded_t dom_models_preloaded_data;

#define X(cb_name, cb_key, cb_type) {cb_key, cb_type},
const dom_models_preloaded_schema_entry_t dom_models_preloaded_schema[DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT] = {
    DOMAIN_MODELS_PRELOADED_SCHEMA(X)
};
#undef X

/* Helper Function Prototypes */

static dom_models_error_t load_default(void);
static dom_models_error_t load_device_id(void);
static dom_models_error_t load_string(char** out, const char* value);
static dom_models_error_t load_nvs_string(nvs_handle_t nvs, const char* key, char** out);
static dom_models_error_t error_from_esp(esp_err_t err);
static uint64_t           device_id_from_base_mac(const uint8_t mac[6]);
static void               clear_preloaded(void);

/* Implementations */

dom_models_error_t cmp_main_preloaded_load_from_nvs(nvs_handle_t nvs) {
    if (!nvs) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    dom_models_error_t err = load_default();
    if (err != DOMAIN_MODELS_ERROR_OK) {
        clear_preloaded();
        return err;
    }

    err = load_nvs_string(nvs, DOMAIN_MODELS_PRELOADED_MQTT_PROTO_KEY, &dom_models_preloaded_data.mqtt_proto);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        clear_preloaded();
        return err;
    }

    err = load_nvs_string(nvs, DOMAIN_MODELS_PRELOADED_MQTT_HOST_KEY, &dom_models_preloaded_data.mqtt_host);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        clear_preloaded();
        return err;
    }

    err = load_nvs_string(nvs, DOMAIN_MODELS_PRELOADED_MQTT_PORT_KEY, &dom_models_preloaded_data.mqtt_port);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        clear_preloaded();
        return err;
    }

    err = load_nvs_string(nvs, DOMAIN_MODELS_PRELOADED_MQTT_USER_KEY, &dom_models_preloaded_data.mqtt_user);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        clear_preloaded();
        return err;
    }

    err = load_nvs_string(nvs, DOMAIN_MODELS_PRELOADED_MQTT_PASS_KEY, &dom_models_preloaded_data.mqtt_pass);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        clear_preloaded();
        return err;
    }

    uint32_t  rst_aft_ms = DEFAULT_SYSTEM_RESTART_AFTER_MS;
    esp_err_t nvs_err    = nvs_get_u32(nvs, DOMAIN_MODELS_PRELOADED_SYSTEM_RESTART_AFTER_MS_KEY, &rst_aft_ms);
    if (nvs_err == ESP_OK) {
        dom_models_preloaded_data.system_restart_after_ms = rst_aft_ms;
    } else if (nvs_err != ESP_ERR_NVS_NOT_FOUND) {
        clear_preloaded();
        return error_from_esp(nvs_err);
    }

    uint8_t wifi_try_init = DEFAULT_WIFI_STA_TRY_CONNECT_ON_INIT ? 1 : 0;
    nvs_err               = nvs_get_u8(nvs, DOMAIN_MODELS_PRELOADED_WIFI_STA_TRY_CONNECT_ON_INIT_KEY, &wifi_try_init);
    if (nvs_err == ESP_OK) {
        dom_models_preloaded_data.wifi_sta_try_connect_on_init = wifi_try_init != 0;
    } else if (nvs_err != ESP_ERR_NVS_NOT_FOUND) {
        clear_preloaded();
        return error_from_esp(nvs_err);
    }

#ifdef COMPOSITION_MAIN_CONFIG_PRELOADED_WIFI_AP_SSID_USE_DEVICE_ID
    err = apply_wifi_ap_ssid_device_id_suffix();
    if (err != DOMAIN_MODELS_ERROR_OK) {
        clear_preloaded();
        return err;
    }
#endif /* COMPOSITION_MAIN_CONFIG_PRELOADED_WIFI_AP_SSID_USE_DEVICE_ID */

    return DOMAIN_MODELS_ERROR_OK;
}

void cmp_main_preloaded_free() {
    clear_preloaded();
}

/* Helper Function Implementations */

static dom_models_error_t load_default(void) {
    clear_preloaded();

    dom_models_error_t err = load_device_id();
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = load_string(&dom_models_preloaded_data.mqtt_proto, DEFAULT_MQTT_PROTO);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = load_string(&dom_models_preloaded_data.mqtt_host, DEFAULT_MQTT_HOST);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = load_string(&dom_models_preloaded_data.mqtt_port, DEFAULT_MQTT_PORT);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = load_string(&dom_models_preloaded_data.mqtt_user, DEFAULT_MQTT_USER);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = load_string(&dom_models_preloaded_data.mqtt_pass, DEFAULT_MQTT_PASS);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    dom_models_preloaded_data.system_restart_after_ms = DEFAULT_SYSTEM_RESTART_AFTER_MS;

    dom_models_preloaded_data.wifi_sta_try_connect_on_init = DEFAULT_WIFI_STA_TRY_CONNECT_ON_INIT;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t load_device_id(void) {
    uint8_t   mac[6];
    esp_err_t err = esp_base_mac_addr_get(mac);
    if (err != ESP_OK) {
        return error_from_esp(err);
    }

    dom_models_preloaded_data.device_id = device_id_from_base_mac(mac);

    char device_id_str[DEVICE_ID_STR_LEN + 1];
    int  written = snprintf(
        device_id_str,
        sizeof(device_id_str),
        "%02X%02X%02X%02X%02X%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
    );
    if (written != DEVICE_ID_STR_LEN) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    return load_string(&dom_models_preloaded_data.device_id_str, device_id_str);
}

static dom_models_error_t load_string(char** out, const char* value) {
    if (!out || !value) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    size_t len  = strlen(value) + 1;
    char*  next = (char*)malloc(len);
    if (!next) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    memcpy(next, value, len);

    free(*out);
    *out = next;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t load_nvs_string(nvs_handle_t nvs, const char* key, char** out) {
    if (!nvs || !key || key[0] == '\0' || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    size_t    len = 0;
    esp_err_t err = nvs_get_str(nvs, key, NULL, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return DOMAIN_MODELS_ERROR_OK;
    }
    if (err != ESP_OK) {
        return error_from_esp(err);
    }

    char* next = (char*)malloc(len);
    if (!next) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    err = nvs_get_str(nvs, key, next, &len);
    if (err != ESP_OK) {
        free(next);
        return error_from_esp(err);
    }

    free(*out);
    *out = next;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t error_from_esp(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return DOMAIN_MODELS_ERROR_OK;
        case ESP_ERR_NO_MEM:
            return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
        case ESP_ERR_INVALID_ARG:
        case ESP_ERR_NVS_INVALID_HANDLE:
            return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        case ESP_ERR_INVALID_MAC:
        case ESP_ERR_INVALID_STATE:
            return DOMAIN_MODELS_ERROR_BAD_STATE;
        case ESP_ERR_TIMEOUT:
            return DOMAIN_MODELS_ERROR_TIMEOUT;
        case ESP_ERR_NOT_SUPPORTED:
            return DOMAIN_MODELS_ERROR_NOT_SUPPORTED;
        default:
            return DOMAIN_MODELS_ERROR_FAILURE;
    }
}

static uint64_t device_id_from_base_mac(const uint8_t mac[6]) {
    return ((uint64_t)mac[0] << 40) |
           ((uint64_t)mac[1] << 32) |
           ((uint64_t)mac[2] << 24) |
           ((uint64_t)mac[3] << 16) |
           ((uint64_t)mac[4] << 8) |
           ((uint64_t)mac[5]);
}

static void clear_preloaded(void) {
    free(dom_models_preloaded_data.device_id_str);
    free(dom_models_preloaded_data.mqtt_proto);
    free(dom_models_preloaded_data.mqtt_host);
    free(dom_models_preloaded_data.mqtt_port);
    free(dom_models_preloaded_data.mqtt_user);
    free(dom_models_preloaded_data.mqtt_pass);

    memset(&dom_models_preloaded_data, 0, sizeof(dom_models_preloaded_t));
}
