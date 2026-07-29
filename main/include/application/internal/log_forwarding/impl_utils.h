#ifndef APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_UTILS_H
#define APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_UTILS_H

#include "application/internal/log_forwarding/impl_types.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t app_internal_log_forwarding_impl_validate_cfg(const app_internal_log_forwarding_impl_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_LOG_FORWARDING_IMPL_UTILS_H */
