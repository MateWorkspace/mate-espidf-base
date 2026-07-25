#ifndef INFRASTRUCTURE_SYSTEM_UPDATE_STUB_IMPL_TYPES_H
#define INFRASTRUCTURE_SYSTEM_UPDATE_STUB_IMPL_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#include "domain/models/error.h"
#include "domain/models/update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool                     update_available;
    dom_models_update_info_t update_info;
    dom_models_error_t       update_result;
    dom_models_error_t       validate_result;
    dom_models_error_t       rollback_result;
} inf_system_update_stub_impl_cfg_t;

#define INF_SYSTEM_UPDATE_STUB_IMPL_CFG_DEFAULT()   \
    {                                               \
        .update_available = false,                  \
        .update_result    = DOMAIN_MODELS_ERROR_OK, \
        .validate_result  = DOMAIN_MODELS_ERROR_OK, \
        .rollback_result  = DOMAIN_MODELS_ERROR_OK, \
    }

typedef struct {
    bool                     update_available;
    dom_models_update_info_t update_info;
    dom_models_error_t       update_result;
    dom_models_error_t       validate_result;
    dom_models_error_t       rollback_result;
    size_t                   update_cnt;
    size_t                   validate_cnt;
    size_t                   rollback_cnt;
} inf_system_update_stub_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_UPDATE_STUB_IMPL_TYPES_H */
