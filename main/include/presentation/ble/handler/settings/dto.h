#ifndef PRESENTATION_BLE_HANDLER_SETTINGS_DTO_H
#define PRESENTATION_BLE_HANDLER_SETTINGS_DTO_H

#include <stdbool.h>
#include <stddef.h>

#include "domain/models/error.h"
#include "domain/usecases/internal/settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/* JSON encode/decode for the settings BLE service. mqtt_pass is
   deliberately never included in the encoded snapshot (only whether one is
   set) - this device has no BLE pairing/bonding configured, so anything
   BLE-readable is readable by any nearby central. */

size_t pres_ble_handler_settings_dto_encode_snapshot(
    const dom_usecases_internal_settings_snapshot_t* snapshot,
    char*                                             buf,
    size_t                                             buf_cap
);

size_t pres_ble_handler_settings_dto_encode_restart_required(bool restart_required, char* buf, size_t buf_cap);

dom_models_error_t pres_ble_handler_settings_dto_decode_update(
    const char*                                          json,
    size_t                                                json_len,
    dom_usecases_internal_settings_preloaded_update_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SETTINGS_DTO_H */
