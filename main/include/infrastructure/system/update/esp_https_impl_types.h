#ifndef INFRASTRUCTURE_SYSTEM_UPDATE_ESP_HTTPS_IMPL_TYPES_H
#define INFRASTRUCTURE_SYSTEM_UPDATE_ESP_HTTPS_IMPL_TYPES_H

#include <stdbool.h>

#include "esp_ota_ops.h"
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int         http_timeout_ms;
    int         http_read_buffer_size;
    int         max_empty_read_count;
    const char* cert_pem;
    bool        keep_alive_enable;
    bool        skip_cert_common_name_check;
} inf_system_update_esp_https_impl_cfg_t;

#define INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_DEFAULT_HTTP_TIMEOUT_MS       30000
#define INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_DEFAULT_HTTP_READ_BUFFER_SIZE 4096
#define INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_DEFAULT_MAX_EMPTY_READ_COUNT  100

#define INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_CFG_DEFAULT()                                                 \
    {                                                                                                  \
        .http_timeout_ms             = INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_DEFAULT_HTTP_TIMEOUT_MS,       \
        .http_read_buffer_size       = INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_DEFAULT_HTTP_READ_BUFFER_SIZE, \
        .max_empty_read_count        = INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_DEFAULT_MAX_EMPTY_READ_COUNT,  \
        .cert_pem                    = NULL,                                                           \
        .keep_alive_enable           = true,                                                           \
        .skip_cert_common_name_check = false,                                                          \
    }

typedef struct {
    inf_system_update_esp_https_impl_cfg_t cfg;
    const esp_partition_t*                 update_partition;
    esp_ota_handle_t                       update_handle;
    bool                                   ota_started;
} inf_system_update_esp_https_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_UPDATE_ESP_HTTPS_IMPL_TYPES_H */
