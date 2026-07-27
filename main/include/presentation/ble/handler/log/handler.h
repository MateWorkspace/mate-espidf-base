#ifndef PRESENTATION_BLE_HANDLER_LOG_HANDLER_H
#define PRESENTATION_BLE_HANDLER_LOG_HANDLER_H

#include "domain/models/error.h"
#include "presentation/ble/handler/log/types.h"

#ifdef __cplusplus
extern "C" {
#endif

pres_ble_handler_log_t* pres_ble_handler_log_new(const pres_ble_handler_log_cfg_t* cfg);

void pres_ble_handler_log_delete(pres_ble_handler_log_t* self);

/* Adds this feature's GATT service to cfg.gatt_registry, subscribes to
   cfg.logger's add_callback and cfg.host's GAP event fan-out, and starts
   the dedicated ble_log FreeRTOS task that owns every NimBLE call this
   feature makes - see types.h and handler.c for why logging calls
   themselves must never touch NimBLE directly. */
dom_models_error_t pres_ble_handler_log_init(pres_ble_handler_log_t* self);

void pres_ble_handler_log_deinit(pres_ble_handler_log_t* self);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_LOG_HANDLER_H */
