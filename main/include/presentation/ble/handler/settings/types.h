#ifndef PRESENTATION_BLE_HANDLER_SETTINGS_TYPES_H
#define PRESENTATION_BLE_HANDLER_SETTINGS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/usecases/internal/settings.h"
#include "presentation/ble/gatt/registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dom_contracts_logger_leveled_t*   logger;
    dom_usecases_internal_settings_t* settings;
    pres_ble_gatt_registry_t*         gatt_registry;
} pres_ble_handler_settings_cfg_t;

typedef struct pres_ble_handler_settings_t {
    pres_ble_handler_settings_cfg_t cfg;
    bool                             registered;
    uint16_t                         restart_required_chr_hdl;
} pres_ble_handler_settings_t;

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SETTINGS_TYPES_H */
