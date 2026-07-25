#ifndef INFRASTRUCTURE_REPOSITORY_WIFI_NVS_IMPL_TYPES_H
#define INFRASTRUCTURE_REPOSITORY_WIFI_NVS_IMPL_TYPES_H

#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t nvs;
} inf_repository_wifi_nvs_impl_cfg_t;

typedef struct {
    inf_repository_wifi_nvs_impl_cfg_t cfg;
} inf_repository_wifi_nvs_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_REPOSITORY_WIFI_NVS_IMPL_TYPES_H */
