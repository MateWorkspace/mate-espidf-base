#ifndef APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_H
#define APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_H

#include "application/internal/log_forwarding/impl_types.h"
#include "domain/usecases/internal/log_forwarding.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_usecases_internal_log_forwarding_t* app_internal_log_forwarding_impl_new(const app_internal_log_forwarding_impl_cfg_t* cfg);

void app_internal_log_forwarding_impl_delete(dom_usecases_internal_log_forwarding_t* self);

dom_models_error_t app_internal_log_forwarding_impl_init(dom_usecases_internal_log_forwarding_t* self);

void app_internal_log_forwarding_impl_deinit(dom_usecases_internal_log_forwarding_t* self);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_H */
