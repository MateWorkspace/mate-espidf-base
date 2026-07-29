#ifndef APPLICATION_INTERNAL_SETTINGS_IMPL_TYPES_H
#define APPLICATION_INTERNAL_SETTINGS_IMPL_TYPES_H

#include <stdbool.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/system/restart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dom_contracts_logger_leveled_t*       logger;
    dom_contracts_repository_preloaded_t* preloaded_repository;
    dom_contracts_system_restart_t*       system_restart;
} app_internal_settings_impl_cfg_t;

typedef struct {
    app_internal_settings_impl_cfg_t cfg;
    bool                             restart_required;
} app_internal_settings_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_SETTINGS_IMPL_TYPES_H */
