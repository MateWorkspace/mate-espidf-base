#ifndef INFRASTRUCTURE_REPOSITORY_WIFI_NVS_IMPL_H
#define INFRASTRUCTURE_REPOSITORY_WIFI_NVS_IMPL_H

#include "domain/contracts/repository/wifi.h"
#include "infrastructure/repository/wifi/nvs_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_repository_wifi_t* inf_repository_wifi_nvs_impl_new(const inf_repository_wifi_nvs_impl_cfg_t* cfg);

void inf_repository_wifi_nvs_impl_delete(dom_contracts_repository_wifi_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_REPOSITORY_WIFI_NVS_IMPL_H */
