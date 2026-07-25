#ifndef INFRASTRUCTURE_REPOSITORY_WIFI_STUB_IMPL_UTILS_H
#define INFRASTRUCTURE_REPOSITORY_WIFI_STUB_IMPL_UTILS_H

#include "domain/models/error.h"
#include "domain/models/wifi.h"
#include "infrastructure/repository/wifi/stub_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_repository_wifi_stub_impl_validate_credential(const dom_models_wifi_sta_connect_config_t* credential);

dom_models_error_t inf_repository_wifi_stub_impl_set_credential(
    inf_repository_wifi_stub_impl_ctx_t*        ctx,
    const dom_models_wifi_sta_connect_config_t* credential
);

dom_models_error_t inf_repository_wifi_stub_impl_load_cfg(
    inf_repository_wifi_stub_impl_ctx_t*       ctx,
    const inf_repository_wifi_stub_impl_cfg_t* cfg
);

void inf_repository_wifi_stub_impl_clear(inf_repository_wifi_stub_impl_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_REPOSITORY_WIFI_STUB_IMPL_UTILS_H */
