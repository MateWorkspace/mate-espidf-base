#ifndef INFRASTRUCTURE_SYSTEM_RESTART_ESP_IMPL_H
#define INFRASTRUCTURE_SYSTEM_RESTART_ESP_IMPL_H

#include "domain/contracts/system/restart.h"
#include "infrastructure/system/restart/esp_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_system_restart_t* inf_system_restart_esp_impl_new(const inf_system_restart_esp_impl_cfg_t* cfg);

void inf_system_restart_esp_impl_delete(dom_contracts_system_restart_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_RESTART_ESP_IMPL_H */
