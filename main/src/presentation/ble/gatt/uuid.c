#include "presentation/ble/gatt/uuid.h"

/* Derives the 16 raw bytes BLE_UUID128_INIT expects (reverse of the
   human-readable string's byte order) for the shared vendor base
   4d415445-SSSS-4700-CCCC-000000000000. Verified by hand against NimBLE's
   own worked example before use - getting this backwards silently produces
   a UUID that never matches what a client thinks it's addressing. */
#define PRES_BLE_GATT_UUID128(svc_hi, svc_lo, chr_hi, chr_lo)          \
    BLE_UUID128_INIT(                                                 \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, (chr_lo), (chr_hi), 0x00, \
        0x47, (svc_lo), (svc_hi), 0x45, 0x54, 0x41, 0x4d               \
    )

/* Settings service (0x0001) */
const ble_uuid128_t pres_ble_gatt_uuid_settings_service              = PRES_BLE_GATT_UUID128(0x00, 0x01, 0x00, 0x00);
const ble_uuid128_t pres_ble_gatt_uuid_settings_data_chr              = PRES_BLE_GATT_UUID128(0x00, 0x01, 0x00, 0x01);
const ble_uuid128_t pres_ble_gatt_uuid_settings_update_chr            = PRES_BLE_GATT_UUID128(0x00, 0x01, 0x00, 0x02);
const ble_uuid128_t pres_ble_gatt_uuid_settings_restart_required_chr  = PRES_BLE_GATT_UUID128(0x00, 0x01, 0x00, 0x03);
const ble_uuid128_t pres_ble_gatt_uuid_settings_restart_chr           = PRES_BLE_GATT_UUID128(0x00, 0x01, 0x00, 0x04);

/* WiFi manager service (0x0002) */
const ble_uuid128_t pres_ble_gatt_uuid_wifi_service                    = PRES_BLE_GATT_UUID128(0x00, 0x02, 0x00, 0x00);
const ble_uuid128_t pres_ble_gatt_uuid_wifi_status_chr                 = PRES_BLE_GATT_UUID128(0x00, 0x02, 0x00, 0x01);
const ble_uuid128_t pres_ble_gatt_uuid_wifi_connect_chr                = PRES_BLE_GATT_UUID128(0x00, 0x02, 0x00, 0x02);
const ble_uuid128_t pres_ble_gatt_uuid_wifi_command_chr                = PRES_BLE_GATT_UUID128(0x00, 0x02, 0x00, 0x03);
const ble_uuid128_t pres_ble_gatt_uuid_wifi_stored_credential_chr      = PRES_BLE_GATT_UUID128(0x00, 0x02, 0x00, 0x04);
const ble_uuid128_t pres_ble_gatt_uuid_wifi_try_connect_on_init_chr    = PRES_BLE_GATT_UUID128(0x00, 0x02, 0x00, 0x05);

/* Log service (0x0003) */
const ble_uuid128_t pres_ble_gatt_uuid_log_service   = PRES_BLE_GATT_UUID128(0x00, 0x03, 0x00, 0x00);
const ble_uuid128_t pres_ble_gatt_uuid_log_message_chr = PRES_BLE_GATT_UUID128(0x00, 0x03, 0x00, 0x01);
const ble_uuid128_t pres_ble_gatt_uuid_log_enabled_chr = PRES_BLE_GATT_UUID128(0x00, 0x03, 0x00, 0x02);
