#ifndef INFRARED_H
#define INFRARED_H

#include <stddef.h>

#include "esp_err.h"
#include "infrared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

infrared_handle_t* infrared_new(const infrared_cfg_t* cfg);

esp_err_t infrared_init(infrared_handle_t* self);

void infrared_deinit(infrared_handle_t* self);

void infrared_delete(infrared_handle_t* self);

esp_err_t infrared_transmit(
    infrared_handle_t*         self,
    const infrared_duration_t* durations,
    size_t                     duration_count
);

#ifdef __cplusplus
}
#endif

#endif /* INFRARED_H */
