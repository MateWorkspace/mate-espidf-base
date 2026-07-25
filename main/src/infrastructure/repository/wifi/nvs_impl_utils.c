#include "infrastructure/repository/wifi/nvs_impl_utils.h"

#include <string.h>

#include "domain/models/wifi.h"
#include "nvs.h"

/* Helper Function Prototypes */

static size_t             bounded_strlen(const char* value, size_t max_len);
static dom_models_error_t validate_credential(const dom_models_wifi_sta_connect_config_t* credential);
static dom_models_error_t erase_key(nvs_handle_t nvs, const char* key);

dom_models_error_t inf_repository_wifi_nvs_impl_error_from_esp(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return DOMAIN_MODELS_ERROR_OK;
        case ESP_ERR_NO_MEM:
            return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
        case ESP_ERR_INVALID_ARG:
        case ESP_ERR_NVS_INVALID_HANDLE:
        case ESP_ERR_NVS_INVALID_NAME:
        case ESP_ERR_NVS_KEY_TOO_LONG:
        case ESP_ERR_NVS_VALUE_TOO_LONG:
        case ESP_ERR_NVS_INVALID_LENGTH:
            return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        case ESP_ERR_NVS_NOT_FOUND:
            return DOMAIN_MODELS_ERROR_NOT_FOUND;
        case ESP_ERR_INVALID_STATE:
        case ESP_ERR_NVS_NOT_INITIALIZED:
        case ESP_ERR_NVS_INVALID_STATE:
        case ESP_ERR_NVS_READ_ONLY:
            return DOMAIN_MODELS_ERROR_BAD_STATE;
        case ESP_ERR_NOT_SUPPORTED:
            return DOMAIN_MODELS_ERROR_NOT_SUPPORTED;
        case ESP_ERR_TIMEOUT:
            return DOMAIN_MODELS_ERROR_TIMEOUT;
        default:
            return DOMAIN_MODELS_ERROR_FAILURE;
    }
}

dom_models_error_t inf_repository_wifi_nvs_impl_read_string(nvs_handle_t nvs, const char* key, char* out, size_t out_size) {
    if (!nvs || !key || key[0] == '\0' || !out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    size_t len = out_size;

    esp_err_t err = nvs_get_str(nvs, key, out, &len);
    if (err != ESP_OK) {
        out[0] = '\0';
        return inf_repository_wifi_nvs_impl_error_from_esp(err);
    }

    out[out_size - 1] = '\0';

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_repository_wifi_nvs_impl_set_credential(nvs_handle_t nvs, const dom_models_wifi_sta_connect_config_t* credential) {
    if (!nvs) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    dom_models_error_t err = validate_credential(credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    esp_err_t esp_err = nvs_set_str(nvs, DOM_MODELS_WIFI_STA_CREDENTIAL_SSID_KEY, credential->ssid);
    if (esp_err != ESP_OK) {
        return inf_repository_wifi_nvs_impl_error_from_esp(esp_err);
    }

    esp_err = nvs_set_str(nvs, DOM_MODELS_WIFI_STA_CREDENTIAL_PASS_KEY, credential->password);
    if (esp_err != ESP_OK) {
        return inf_repository_wifi_nvs_impl_error_from_esp(esp_err);
    }

    esp_err = nvs_commit(nvs);
    if (esp_err != ESP_OK) {
        return inf_repository_wifi_nvs_impl_error_from_esp(esp_err);
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_repository_wifi_nvs_impl_clear_credential(nvs_handle_t nvs) {
    if (!nvs) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    dom_models_error_t err = erase_key(nvs, DOM_MODELS_WIFI_STA_CREDENTIAL_SSID_KEY);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = erase_key(nvs, DOM_MODELS_WIFI_STA_CREDENTIAL_PASS_KEY);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    esp_err_t esp_err = nvs_commit(nvs);
    if (esp_err != ESP_OK) {
        return inf_repository_wifi_nvs_impl_error_from_esp(esp_err);
    }

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static size_t bounded_strlen(const char* value, size_t max_len) {
    size_t len = 0;

    if (!value) {
        return 0;
    }

    while (len < max_len && value[len] != '\0') {
        len++;
    }

    return len;
}

static dom_models_error_t validate_credential(const dom_models_wifi_sta_connect_config_t* credential) {
    if (!credential) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    size_t ssid_len = bounded_strlen(credential->ssid, sizeof(credential->ssid));
    if (ssid_len == 0 || ssid_len >= sizeof(credential->ssid)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    size_t password_len = bounded_strlen(credential->password, sizeof(credential->password));
    if (password_len >= sizeof(credential->password)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t erase_key(nvs_handle_t nvs, const char* key) {
    esp_err_t err = nvs_erase_key(nvs, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return DOMAIN_MODELS_ERROR_OK;
    }
    if (err != ESP_OK) {
        return inf_repository_wifi_nvs_impl_error_from_esp(err);
    }

    return DOMAIN_MODELS_ERROR_OK;
}
