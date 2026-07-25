#ifndef INFRASTRUCTURE_REPOSITORY_PRELOADED_NVS_IMPL_UTILS_H
#define INFRASTRUCTURE_REPOSITORY_PRELOADED_NVS_IMPL_UTILS_H

#include <stddef.h>

#include "domain/models/error.h"
#include "esp_err.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_repository_preloaded_nvs_impl_error_from_esp(esp_err_t err);

dom_models_error_t inf_repository_preloaded_nvs_impl_copy_cstr(char* out, size_t out_size, const char* value);

dom_models_error_t inf_repository_preloaded_nvs_impl_set_string(nvs_handle_t nvs, const char* key, char** runtime_value, const char* value);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_REPOSITORY_PRELOADED_NVS_IMPL_UTILS_H */
