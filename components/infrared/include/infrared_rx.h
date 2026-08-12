#ifndef INFRARED_RX_H
#define INFRARED_RX_H

#include "esp_err.h"
#include "infrared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t infrared_rx_init(infrared_handle_t* self);

void infrared_rx_deinit(infrared_handle_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRARED_RX_H */
