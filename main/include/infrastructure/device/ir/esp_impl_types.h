#ifndef INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_TYPES_H
#define INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_TYPES_H

#include "domain/models/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INF_DEVICE_IR_ESP_IMPL_RX_GPIO GPIO_NUM_4
#define INF_DEVICE_IR_ESP_IMPL_TX_GPIO GPIO_NUM_5

typedef struct {
    dom_models_ir_receive_cb_t receive_cb;
    void*                      receive_cb_ctx;
    void*                      infrared; /* infrared_handle_t*, opaque here so this
                                             header doesn't leak the component's
                                             ESP-IDF-native type into domain-adjacent
                                             includes */
    bool initialized;
} inf_device_ir_esp_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_IR_ESP_IMPL_TYPES_H */
