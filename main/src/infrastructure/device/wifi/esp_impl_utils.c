#include "infrastructure/device/wifi/esp_impl_utils.h"

#include <string.h>

#include "esp_wifi.h"

dom_models_error_t inf_device_wifi_esp_impl_error_from_esp(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return DOMAIN_MODELS_ERROR_OK;
        case ESP_ERR_NO_MEM:
            return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
        case ESP_ERR_INVALID_ARG:
            return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        case ESP_ERR_INVALID_STATE:
        case ESP_ERR_WIFI_NOT_INIT:
        case ESP_ERR_WIFI_NOT_STARTED:
        case ESP_ERR_WIFI_IF:
            return DOMAIN_MODELS_ERROR_BAD_STATE;
        case ESP_ERR_NOT_SUPPORTED:
            return DOMAIN_MODELS_ERROR_NOT_SUPPORTED;
        case ESP_ERR_TIMEOUT:
            return DOMAIN_MODELS_ERROR_TIMEOUT;
        default:
            return DOMAIN_MODELS_ERROR_FAILURE;
    }
}

size_t inf_device_wifi_esp_impl_bounded_strlen(const char* value, size_t max_len) {
    size_t len = 0;

    if (!value) {
        return 0;
    }

    while (len < max_len && value[len] != '\0') {
        len++;
    }

    return len;
}

size_t inf_device_wifi_esp_impl_bounded_byte_strlen(const uint8_t* value, size_t max_len) {
    size_t len = 0;

    if (!value) {
        return 0;
    }

    while (len < max_len && value[len] != '\0') {
        len++;
    }

    return len;
}

void inf_device_wifi_esp_impl_copy_cstr(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) {
        return;
    }

    const char* value = src ? src : "";
    size_t      len   = inf_device_wifi_esp_impl_bounded_strlen(value, dst_size - 1);
    dst[len]          = '\0';

    if (len > 0) {
        memcpy(dst, value, len);
    }
}

void inf_device_wifi_esp_impl_copy_cstr_to_bytes(uint8_t* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0 || !src) {
        return;
    }

    size_t len = inf_device_wifi_esp_impl_bounded_strlen(src, dst_size);
    if (len > 0) {
        memcpy(dst, src, len);
    }
}

void inf_device_wifi_esp_impl_copy_bytes_to_cstr(char* dst, size_t dst_size, const uint8_t* src, size_t src_size) {
    if (!dst || dst_size == 0) {
        return;
    }

    size_t len      = inf_device_wifi_esp_impl_bounded_byte_strlen(src, src_size);
    size_t copy_len = len < dst_size - 1 ? len : dst_size - 1;
    dst[copy_len]   = '\0';

    if (copy_len > 0) {
        memcpy(dst, src, copy_len);
    }
}

void inf_device_wifi_esp_impl_ip4_to_bytes(esp_ip4_addr_t addr, uint8_t out[4]) {
    if (!out) {
        return;
    }

    out[0] = esp_ip4_addr_get_byte(&addr, 0);
    out[1] = esp_ip4_addr_get_byte(&addr, 1);
    out[2] = esp_ip4_addr_get_byte(&addr, 2);
    out[3] = esp_ip4_addr_get_byte(&addr, 3);
}
