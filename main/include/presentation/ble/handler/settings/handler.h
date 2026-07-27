#ifndef PRESENTATION_BLE_HANDLER_SETTINGS_HANDLER_H
#define PRESENTATION_BLE_HANDLER_SETTINGS_HANDLER_H

#include "domain/models/error.h"
#include "presentation/ble/handler/settings/types.h"

#ifdef __cplusplus
extern "C" {
#endif

pres_ble_handler_settings_t* pres_ble_handler_settings_new(const pres_ble_handler_settings_cfg_t* cfg);

void pres_ble_handler_settings_delete(pres_ble_handler_settings_t* self);

/* Adds this feature's GATT service to cfg.gatt_registry. Must be called
   before the shared registry's register_all() (see gatt/registry.h). */
dom_models_error_t pres_ble_handler_settings_init(pres_ble_handler_settings_t* self);

/* NimBLE never allows a registered characteristic to be unregistered, so
   this neuters the access callbacks instead of removing anything - see
   gatt/util.h's disabled_access_callback. */
void pres_ble_handler_settings_deinit(pres_ble_handler_settings_t* self);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SETTINGS_HANDLER_H */
