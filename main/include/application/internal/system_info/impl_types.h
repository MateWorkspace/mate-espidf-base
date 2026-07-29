#ifndef APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_TYPES_H
#define APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_TYPES_H

#include <stdbool.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/system/info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dom_contracts_logger_leveled_t* logger;
    dom_contracts_system_info_t*    system_info;
} app_internal_system_info_impl_cfg_t;

typedef struct {
    app_internal_system_info_impl_cfg_t cfg;
} app_internal_system_info_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_SYSTEM_INFO_IMPL_TYPES_H */
