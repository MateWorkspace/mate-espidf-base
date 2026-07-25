#ifndef INFRASTRUCTURE_SYSTEM_INFO_ESP_IMPL_H
#define INFRASTRUCTURE_SYSTEM_INFO_ESP_IMPL_H

#include "domain/contracts/system/info.h"
#include "infrastructure/system/info/esp_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_system_info_t* inf_system_info_esp_impl_new(const inf_system_info_esp_impl_cfg_t* cfg);

void inf_system_info_esp_impl_delete(dom_contracts_system_info_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_INFO_ESP_IMPL_H */
