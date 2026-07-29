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

#define PRES_BLE_HANDLER_SETTINGS_UPDATE_PAYLOAD_MAX_LEN        384
#define PRES_BLE_HANDLER_SETTINGS_SNAPSHOT_JSON_MAX_LEN         512
#define PRES_BLE_HANDLER_SETTINGS_RESTART_REQUIRED_JSON_MAX_LEN 48

typedef struct {
    dom_contracts_logger_leveled_t*   logger;
    dom_usecases_internal_settings_t* settings;
    pres_ble_gatt_registry_t*         gatt_registry;
} pres_ble_handler_settings_cfg_t;

typedef struct pres_ble_handler_settings_t {
    pres_ble_handler_settings_cfg_t cfg;
    bool                            registered;
    uint16_t                        restart_required_chr_hdl;
    /* Struct-owned (not stack-local) so a read/write access callback -
       which runs on the nimble_host task, whose configured stack is small -
       never has to fit these on its own stack. A stack-local
       char[SNAPSHOT_JSON_MAX_LEN] here previously caused a stack protection
       fault/crash on every settings-data read. */
    char update_payload[PRES_BLE_HANDLER_SETTINGS_UPDATE_PAYLOAD_MAX_LEN];
    char snapshot_json[PRES_BLE_HANDLER_SETTINGS_SNAPSHOT_JSON_MAX_LEN];
    char restart_required_json[PRES_BLE_HANDLER_SETTINGS_RESTART_REQUIRED_JSON_MAX_LEN];
} pres_ble_handler_settings_t;

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SETTINGS_TYPES_H */
