#ifndef APPLICATION_INTERNAL_INFRARED_IMPL_H
#define APPLICATION_INTERNAL_INFRARED_IMPL_H

#include "application/internal/infrared/impl_types.h"
#include "domain/usecases/internal/infrared.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_infrared_t* app_internal_infrared_impl_new(const app_internal_infrared_impl_cfg_t* cfg);

void app_internal_infrared_impl_delete(dom_usecases_internal_infrared_t* self);

dom_models_error_t app_internal_infrared_impl_init(dom_usecases_internal_infrared_t* self);

void app_internal_infrared_impl_deinit(dom_usecases_internal_infrared_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_INFRARED_IMPL_H */
