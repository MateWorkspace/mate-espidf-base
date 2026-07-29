#ifndef PRESENTATION_BLE_HANDLER_LOG_UTILS_H
#define PRESENTATION_BLE_HANDLER_LOG_UTILS_H

#include "domain/models/error.h"
#include "presentation/ble/handler/log/types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_ble_handler_log_validate_cfg(const pres_ble_handler_log_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_LOG_UTILS_H */
