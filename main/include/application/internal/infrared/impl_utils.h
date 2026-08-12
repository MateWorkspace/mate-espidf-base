#ifndef APPLICATION_INTERNAL_INFRARED_IMPL_UTILS_H
#define APPLICATION_INTERNAL_INFRARED_IMPL_UTILS_H

#include <stddef.h>
#include <stdint.h>

#include "application/internal/infrared/impl_types.h"
#include "domain/models/error.h"
#include "domain/models/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_infrared_impl_validate_cfg(const app_internal_infrared_impl_cfg_t* cfg);

/* Rebuilds (level, duration) pairs from a flat alternating raw_data array,
   starting with a mark (level=true) at index 0. out must have capacity for
   raw_data_count entries. */
dom_models_error_t app_internal_infrared_impl_raw_data_to_durations(
    const int32_t*            raw_data,
    size_t                    raw_data_count,
    dom_models_ir_duration_t* out
);

/* Strips the level field, producing the flat alternating array the backend
   expects. out must have capacity for duration_count entries. */
dom_models_error_t app_internal_infrared_impl_durations_to_raw_data(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    int32_t*                        out
);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_INFRARED_IMPL_UTILS_H */
