#include "infrastructure/repository/preloaded/nvs_impl_utils.h"

#include <stdlib.h>
#include <string.h>

#include "nvs.h"

dom_models_error_t inf_repository_preloaded_nvs_impl_error_from_esp(esp_err_t err) {
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

dom_models_error_t inf_repository_preloaded_nvs_impl_copy_cstr(char* out, size_t out_size, const char* value) {
    if (!out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    const char* src = value ? value : "";
    size_t      len = strlen(src);
    if (len >= out_size) {
        len = out_size - 1;
    }

    memcpy(out, src, len);
    out[len] = '\0';

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_repository_preloaded_nvs_impl_set_string(nvs_handle_t nvs, const char* key, char** runtime_value, const char* value) {
    if (!nvs || !key || key[0] == '\0' || !runtime_value || !value) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    size_t len  = strlen(value) + 1;
    char*  next = (char*)malloc(len);
    if (!next) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }
    memcpy(next, value, len);

    esp_err_t err = nvs_set_str(nvs, key, value);
    if (err != ESP_OK) {
        free(next);
        return inf_repository_preloaded_nvs_impl_error_from_esp(err);
    }

    err = nvs_commit(nvs);
    if (err != ESP_OK) {
        free(next);
        return inf_repository_preloaded_nvs_impl_error_from_esp(err);
    }

    free(*runtime_value);
    *runtime_value = next;

    return DOMAIN_MODELS_ERROR_OK;
}
