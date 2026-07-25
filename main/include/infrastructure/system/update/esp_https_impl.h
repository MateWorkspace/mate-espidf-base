#ifndef INFRASTRUCTURE_SYSTEM_UPDATE_ESP_HTTPS_IMPL_H
#define INFRASTRUCTURE_SYSTEM_UPDATE_ESP_HTTPS_IMPL_H

#include "domain/contracts/system/update.h"
#include "infrastructure/system/update/esp_https_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_system_update_t* inf_system_update_esp_https_impl_new(
    const inf_system_update_esp_https_impl_cfg_t* cfg
);

void inf_system_update_esp_https_impl_delete(dom_contracts_system_update_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_UPDATE_ESP_HTTPS_IMPL_H */
