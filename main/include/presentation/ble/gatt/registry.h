#ifndef PRESENTATION_BLE_GATT_REGISTRY_H
#define PRESENTATION_BLE_GATT_REGISTRY_H

#include "domain/models/error.h"
#include "host/ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared NimBLE GATT registration point. Every feature (settings,
   wifi_manager, log) still owns its own static-lifetime
   struct ble_gatt_svc_def (NimBLE retains a raw pointer into it forever
   once registered - that part cannot be shared), but instead of each
   feature separately calling ble_gatts_count_cfg/ble_gatts_add_svcs, they
   hand their service def pointer to this registry via
   pres_ble_gatt_registry_add_service(), and composition calls
   pres_ble_gatt_registry_register_all() exactly once, after every feature
   has registered, before the BLE host starts. This is the concrete fix for
   the duplicated per-feature registration boilerplate idf-base has. */

typedef struct pres_ble_gatt_registry_t pres_ble_gatt_registry_t;

pres_ble_gatt_registry_t* pres_ble_gatt_registry_new(void);

void pres_ble_gatt_registry_delete(pres_ble_gatt_registry_t* self);

dom_models_error_t pres_ble_gatt_registry_add_service(
    pres_ble_gatt_registry_t*      self,
    const struct ble_gatt_svc_def* svc
);

dom_models_error_t pres_ble_gatt_registry_register_all(pres_ble_gatt_registry_t* self);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_GATT_REGISTRY_H */
