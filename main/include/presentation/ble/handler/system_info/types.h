#ifndef PRESENTATION_BLE_HANDLER_SYSTEM_INFO_TYPES_H
#define PRESENTATION_BLE_HANDLER_SYSTEM_INFO_TYPES_H

#include <stdbool.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/usecases/internal/system_info.h"
#include "presentation/ble/gatt/registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_BLE_HANDLER_SYSTEM_INFO_INFO_JSON_MAX_LEN          192
#define PRES_BLE_HANDLER_SYSTEM_INFO_CONFIG_SCHEMA_JSON_MAX_LEN 256

typedef struct {
    dom_contracts_logger_leveled_t*      logger;
    dom_usecases_internal_system_info_t* system_info;
    pres_ble_gatt_registry_t*            gatt_registry;
} pres_ble_handler_system_info_cfg_t;

typedef struct pres_ble_handler_system_info_t {
    pres_ble_handler_system_info_cfg_t cfg;
    bool                                registered;
    /* Struct-owned (not stack-local) for the same reason as
       presentation/ble/handler/settings/types.h's buffers: a read access
       callback runs on the nimble_host task, whose configured stack is
       small - a stack-local buffer here previously caused a stack
       protection fault/crash on settings-data reads. */
    char info_json[PRES_BLE_HANDLER_SYSTEM_INFO_INFO_JSON_MAX_LEN];
    char config_schema_json[PRES_BLE_HANDLER_SYSTEM_INFO_CONFIG_SCHEMA_JSON_MAX_LEN];
} pres_ble_handler_system_info_t;

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SYSTEM_INFO_TYPES_H */
