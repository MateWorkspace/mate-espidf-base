#ifndef PRESENTATION_BLE_HANDLER_WIFI_MANAGER_DTO_H
#define PRESENTATION_BLE_HANDLER_WIFI_MANAGER_DTO_H

#include <stddef.h>

#include "domain/models/error.h"
#include "domain/models/wifi.h"
#include "domain/usecases/internal/wifi_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t pres_ble_handler_wifi_manager_dto_encode_status(
    const dom_usecases_internal_wifi_manager_status_t* status,
    char*                                                buf,
    size_t                                                buf_cap
);

/* Never includes the stored password, only availability + ssid - same
   "no secrets over an unpaired BLE link" principle as the settings
   service's mqtt_pass_set. */
size_t pres_ble_handler_wifi_manager_dto_encode_stored_credential(
    const dom_usecases_internal_wifi_manager_stored_sta_t* stored,
    char*                                                    buf,
    size_t                                                    buf_cap
);

dom_models_error_t pres_ble_handler_wifi_manager_dto_decode_connect(
    const char*                            json,
    size_t                                  json_len,
    dom_models_wifi_sta_connect_config_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_WIFI_MANAGER_DTO_H */
