#ifndef INFRASTRUCTURE_REPOSITORY_WIFI_STUB_IMPL_TYPES_H
#define INFRASTRUCTURE_REPOSITORY_WIFI_STUB_IMPL_TYPES_H

#include <stdbool.h>

#include "domain/models/wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool                                 credential_available;
    dom_models_wifi_sta_connect_config_t credential;
} inf_repository_wifi_stub_impl_cfg_t;

#define INF_REPOSITORY_WIFI_STUB_IMPL_CFG_DEFAULT() \
    {                                               \
        .credential_available = false,              \
    }

typedef struct {
    bool                                 credential_available;
    dom_models_wifi_sta_connect_config_t credential;
} inf_repository_wifi_stub_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_REPOSITORY_WIFI_STUB_IMPL_TYPES_H */
