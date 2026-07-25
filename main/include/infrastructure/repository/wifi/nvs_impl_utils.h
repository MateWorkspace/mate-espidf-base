#ifndef INFRASTRUCTURE_REPOSITORY_WIFI_NVS_IMPL_UTILS_H
#define INFRASTRUCTURE_REPOSITORY_WIFI_NVS_IMPL_UTILS_H

#include <stddef.h>

#include "domain/models/error.h"
#include "domain/models/wifi.h"
#include "esp_err.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_repository_wifi_nvs_impl_error_from_esp(esp_err_t err);

dom_models_error_t inf_repository_wifi_nvs_impl_read_string(nvs_handle_t nvs, const char* key, char* out, size_t out_size);

dom_models_error_t inf_repository_wifi_nvs_impl_set_credential(nvs_handle_t nvs, const dom_models_wifi_sta_connect_config_t* credential);

dom_models_error_t inf_repository_wifi_nvs_impl_clear_credential(nvs_handle_t nvs);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_REPOSITORY_WIFI_NVS_IMPL_UTILS_H */
