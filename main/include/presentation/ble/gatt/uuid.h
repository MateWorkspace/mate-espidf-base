#ifndef PRESENTATION_BLE_GATT_UUID_H
#define PRESENTATION_BLE_GATT_UUID_H

#include "host/ble_uuid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Single source of truth for every BLE UUID this device exposes. All
   custom UUIDs share the vendor base 4d415445-SSSS-4700-CCCC-000000000000
   ("MATE" in ASCII hex at the start), where SSSS identifies the service and
   CCCC identifies the characteristic within it (0000 for the service UUID
   itself). Keeping every UUID here, instead of hardcoded per-feature like
   idf-base's BLE code, is what lets a new feature be added without
   re-deriving byte-reversed UUID literals by hand. */

/* Settings service (0x0001) */
extern const ble_uuid128_t pres_ble_gatt_uuid_settings_service;
extern const ble_uuid128_t pres_ble_gatt_uuid_settings_data_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_settings_update_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_settings_restart_required_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_settings_restart_chr;

/* WiFi manager service (0x0002) */
extern const ble_uuid128_t pres_ble_gatt_uuid_wifi_service;
extern const ble_uuid128_t pres_ble_gatt_uuid_wifi_status_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_wifi_connect_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_wifi_command_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_wifi_stored_credential_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_wifi_try_connect_on_init_chr;

/* Log service (0x0003) */
extern const ble_uuid128_t pres_ble_gatt_uuid_log_service;
extern const ble_uuid128_t pres_ble_gatt_uuid_log_message_chr;
extern const ble_uuid128_t pres_ble_gatt_uuid_log_enabled_chr;

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_GATT_UUID_H */
