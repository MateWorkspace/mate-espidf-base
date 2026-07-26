#ifndef APPLICATION_INTERNAL_OTA_IMPL_TYPES_H
#define APPLICATION_INTERNAL_OTA_IMPL_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/system/restart.h"
#include "domain/contracts/system/update.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_INTERNAL_OTA_IMPL_DEFAULT_RESTART_DELAY_MS 5000

typedef struct {
    dom_contracts_logger_leveled_t* logger;
    dom_contracts_system_update_t*  system_update;
    dom_contracts_system_restart_t* system_restart;
    uint32_t                        restart_delay_ms;
} app_internal_ota_impl_cfg_t;

typedef struct {
    app_internal_ota_impl_cfg_t cfg;
    bool                        updating;
    dom_models_error_t          last_result;
    int                         last_progress_percent;
} app_internal_ota_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_OTA_IMPL_TYPES_H */
