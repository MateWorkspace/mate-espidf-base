#ifndef INFRARED_TX_H
#define INFRARED_TX_H

#include <stddef.h>

#include "esp_err.h"
#include "infrared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t infrared_tx_init(infrared_handle_t* self);

void infrared_tx_deinit(infrared_handle_t* self);

esp_err_t infrared_tx_transmit(
    infrared_handle_t*         self,
    const infrared_duration_t* durations,
    size_t                     duration_count
);

#ifdef __cplusplus
}
#endif

#endif /* INFRARED_TX_H */
