#ifndef INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_H
#define INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_H

#include "domain/contracts/device/ir.h"
#include "infrastructure/device/ir/esp_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_device_ir_t* inf_device_ir_esp_impl_new(void* unused_cfg);

void inf_device_ir_esp_impl_delete(dom_contracts_device_ir_t* self);

dom_models_error_t inf_device_ir_esp_impl_init(dom_contracts_device_ir_t* self);

void inf_device_ir_esp_impl_deinit(dom_contracts_device_ir_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_H */
