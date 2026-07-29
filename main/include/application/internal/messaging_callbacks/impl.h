#ifndef APPLICATION_INTERNAL_MESSAGING_CALLBACKS_IMPL_H
#define APPLICATION_INTERNAL_MESSAGING_CALLBACKS_IMPL_H

#include "application/internal/messaging_callbacks/impl_types.h"
#include "domain/usecases/internal/messaging_callbacks.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_messaging_callbacks_t* app_internal_messaging_callbacks_impl_new(const app_internal_messaging_callbacks_impl_cfg_t* cfg);

void app_internal_messaging_callbacks_impl_delete(dom_usecases_internal_messaging_callbacks_t* self);

dom_models_error_t app_internal_messaging_callbacks_impl_init(dom_usecases_internal_messaging_callbacks_t* self);

void app_internal_messaging_callbacks_impl_deinit(dom_usecases_internal_messaging_callbacks_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_MESSAGING_CALLBACKS_IMPL_H */
