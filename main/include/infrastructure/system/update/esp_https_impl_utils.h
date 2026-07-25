#ifndef INFRASTRUCTURE_SYSTEM_UPDATE_ESP_HTTPS_IMPL_UTILS_H
#define INFRASTRUCTURE_SYSTEM_UPDATE_ESP_HTTPS_IMPL_UTILS_H

#include <stdbool.h>
#include <stdint.h>

#include "domain/models/error.h"
#include "domain/models/update.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN 64

dom_models_error_t inf_system_update_esp_https_impl_error_from_esp(esp_err_t err);

bool inf_system_update_esp_https_impl_update_info_valid(
    const dom_models_update_info_t* update_info
);

bool inf_system_update_esp_https_impl_normalize_sha256_hex(
    const char* checksum,
    char        expected[INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN + 1]
);

void inf_system_update_esp_https_impl_sha256_to_hex(
    const uint8_t digest[32],
    char          hex[INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN + 1]
);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_UPDATE_ESP_HTTPS_IMPL_UTILS_H */
