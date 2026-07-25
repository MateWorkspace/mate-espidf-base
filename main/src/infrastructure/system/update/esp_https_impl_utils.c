#include "infrastructure/system/update/esp_https_impl_utils.h"

#include <ctype.h>
#include <string.h>

#include "domain/models/error.h"
#include "esp_err.h"

dom_models_error_t inf_system_update_esp_https_impl_error_from_esp(esp_err_t err) {
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

bool inf_system_update_esp_https_impl_update_info_valid(
    const dom_models_update_info_t* update_info
) {
    if (!update_info || update_info->firmware_url[0] == '\0' || update_info->firmware_size == 0) {
        return false;
    }

    char expected[INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN + 1];
    return inf_system_update_esp_https_impl_normalize_sha256_hex(update_info->firmware_checksum, expected);
}

bool inf_system_update_esp_https_impl_normalize_sha256_hex(
    const char* checksum,
    char        expected[INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN + 1]
) {
    if (!checksum || strlen(checksum) != INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN) {
        return false;
    }

    for (size_t i = 0; i < INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN; i++) {
        unsigned char c = (unsigned char)checksum[i];
        if (!isxdigit(c)) {
            return false;
        }
        expected[i] = (char)tolower(c);
    }
    expected[INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN] = '\0';

    return true;
}

void inf_system_update_esp_https_impl_sha256_to_hex(
    const uint8_t digest[32],
    char          hex[INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN + 1]
) {
    static const char* chars = "0123456789abcdef";

    for (size_t i = 0; i < 32; i++) {
        hex[i * 2U]        = chars[(digest[i] >> 4) & 0x0F];
        hex[(i * 2U) + 1U] = chars[digest[i] & 0x0F];
    }
    hex[INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN] = '\0';
}
