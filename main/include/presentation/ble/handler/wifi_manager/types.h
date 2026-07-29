#ifndef PRESENTATION_BLE_HANDLER_WIFI_MANAGER_TYPES_H
#define PRESENTATION_BLE_HANDLER_WIFI_MANAGER_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/usecases/internal/wifi_manager.h"
#include "presentation/ble/gatt/registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_STOP           = 0,
    PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_START          = 1,
    PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_CONNECT_STORED = 2,
    PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_DISCONNECT     = 3,
    PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_FORGET_STORED  = 4,
} pres_ble_handler_wifi_manager_command_t;

#define PRES_BLE_HANDLER_WIFI_MANAGER_CONNECT_PAYLOAD_MAX_LEN        160
#define PRES_BLE_HANDLER_WIFI_MANAGER_STATUS_JSON_MAX_LEN            256
#define PRES_BLE_HANDLER_WIFI_MANAGER_STORED_CREDENTIAL_JSON_MAX_LEN 96

typedef struct {
    dom_contracts_logger_leveled_t*       logger;
    dom_usecases_internal_wifi_manager_t* wifi_manager;
    pres_ble_gatt_registry_t*             gatt_registry;
} pres_ble_handler_wifi_manager_cfg_t;

typedef struct pres_ble_handler_wifi_manager_t {
    pres_ble_handler_wifi_manager_cfg_t cfg;
    bool                                registered;
    bool                                status_cb_subscribed;
    uint16_t                            status_chr_hdl;
    /* Struct-owned, not stack-local - see settings/types.h's comment on why
       (nimble_host task's small configured stack). */
    char connect_payload[PRES_BLE_HANDLER_WIFI_MANAGER_CONNECT_PAYLOAD_MAX_LEN];
    char status_json[PRES_BLE_HANDLER_WIFI_MANAGER_STATUS_JSON_MAX_LEN];
    char stored_credential_json[PRES_BLE_HANDLER_WIFI_MANAGER_STORED_CREDENTIAL_JSON_MAX_LEN];
} pres_ble_handler_wifi_manager_t;

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_WIFI_MANAGER_TYPES_H */
