#include "infrastructure/system/info/esp_impl_utils.h"

#include <stdio.h>
#include <string.h>

#include "domain/models/error.h"
#include "esp_err.h"

/* Helper Function Prototypes */

static size_t bounded_strlen(const char* value, size_t max_len);

dom_models_error_t inf_system_info_esp_impl_error_from_esp(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return DOMAIN_MODELS_ERROR_OK;
        case ESP_ERR_NO_MEM:
            return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
        case ESP_ERR_INVALID_ARG:
            return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        case ESP_ERR_INVALID_STATE:
            return DOMAIN_MODELS_ERROR_BAD_STATE;
        case ESP_ERR_NOT_FOUND:
            return DOMAIN_MODELS_ERROR_NOT_FOUND;
        case ESP_ERR_NOT_SUPPORTED:
            return DOMAIN_MODELS_ERROR_NOT_SUPPORTED;
        case ESP_ERR_TIMEOUT:
            return DOMAIN_MODELS_ERROR_TIMEOUT;
        default:
            return DOMAIN_MODELS_ERROR_FAILURE;
    }
}

void inf_system_info_esp_impl_copy_cstr(char* out, size_t out_size, const char* value) {
    if (!out || out_size == 0) {
        return;
    }

    size_t len = bounded_strlen(value, out_size - 1);
    if (len > 0) {
        memcpy(out, value, len);
    }
    out[len] = '\0';
}

dom_models_error_t inf_system_info_esp_impl_format_mac(
    const uint8_t mac[6],
    char*         out,
    size_t        out_size
) {
    if (!mac || !out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int written = snprintf(
        out,
        out_size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
    );
    if (written != 17 || (size_t)written >= out_size) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

const char* inf_system_info_esp_impl_chip_model_str(esp_chip_model_t model) {
    switch (model) {
        case CHIP_ESP32:
            return "ESP32";
        case CHIP_ESP32S2:
            return "ESP32-S2";
        case CHIP_ESP32S3:
            return "ESP32-S3";
        case CHIP_ESP32C3:
            return "ESP32-C3";
        case CHIP_ESP32C2:
            return "ESP32-C2";
        case CHIP_ESP32C6:
            return "ESP32-C6";
        case CHIP_ESP32H2:
            return "ESP32-H2";
        case CHIP_ESP32P4:
            return "ESP32-P4";
        default:
            return "unknown";
    }
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
