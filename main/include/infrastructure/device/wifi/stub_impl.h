#ifndef INFRASTRUCTURE_DEVICE_WIFI_STUB_IMPL_H
#define INFRASTRUCTURE_DEVICE_WIFI_STUB_IMPL_H

#include "domain/contracts/device/wifi.h"
#include "infrastructure/device/wifi/stub_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_device_wifi_t* inf_device_wifi_stub_impl_new(const inf_device_wifi_stub_impl_cfg_t* cfg);

void inf_device_wifi_stub_impl_delete(dom_contracts_device_wifi_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_WIFI_STUB_IMPL_H */
