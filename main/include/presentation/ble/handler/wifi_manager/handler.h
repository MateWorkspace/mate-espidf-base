#ifndef PRESENTATION_BLE_HANDLER_WIFI_MANAGER_HANDLER_H
#define PRESENTATION_BLE_HANDLER_WIFI_MANAGER_HANDLER_H

#include "domain/models/error.h"
#include "presentation/ble/handler/wifi_manager/types.h"

#ifdef __cplusplus
extern "C" {
#endif

pres_ble_handler_wifi_manager_t* pres_ble_handler_wifi_manager_new(const pres_ble_handler_wifi_manager_cfg_t* cfg);

void pres_ble_handler_wifi_manager_delete(pres_ble_handler_wifi_manager_t* self);

/* Adds this feature's GATT service to cfg.gatt_registry (must happen
   before the registry's register_all()) and subscribes to
   wifi_manager->add_status_callback so WiFi-Status can push live Notify
   updates on real connect/disconnect events. */
dom_models_error_t pres_ble_handler_wifi_manager_init(pres_ble_handler_wifi_manager_t* self);

void pres_ble_handler_wifi_manager_deinit(pres_ble_handler_wifi_manager_t* self);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_WIFI_MANAGER_HANDLER_H */
