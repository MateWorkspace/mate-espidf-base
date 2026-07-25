#ifndef INFRASTRUCTURE_SYSTEM_INFO_ESP_IMPL_UTILS_H
#define INFRASTRUCTURE_SYSTEM_INFO_ESP_IMPL_UTILS_H

#include <stddef.h>
#include <stdint.h>

#include "domain/models/error.h"
#include "esp_chip_info.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_system_info_esp_impl_error_from_esp(esp_err_t err);

void inf_system_info_esp_impl_copy_cstr(char* out, size_t out_size, const char* value);

dom_models_error_t inf_system_info_esp_impl_format_mac(
    const uint8_t mac[6],
    char*         out,
    size_t        out_size
);

const char* inf_system_info_esp_impl_chip_model_str(esp_chip_model_t model);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_INFO_ESP_IMPL_UTILS_H */
