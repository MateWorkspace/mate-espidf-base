#ifndef INFRASTRUCTURE_DEVICE_WIFI_ESP_IMPL_H
#define INFRASTRUCTURE_DEVICE_WIFI_ESP_IMPL_H

#include "domain/contracts/device/wifi.h"
#include "infrastructure/device/wifi/esp_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_device_wifi_t* inf_device_wifi_esp_impl_new(const inf_device_wifi_esp_impl_cfg_t* cfg);

void inf_device_wifi_esp_impl_delete(dom_contracts_device_wifi_t* self);

dom_models_error_t inf_device_wifi_esp_impl_init(dom_contracts_device_wifi_t* self);

dom_models_error_t inf_device_wifi_esp_impl_deinit(dom_contracts_device_wifi_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_WIFI_ESP_IMPL_H */
