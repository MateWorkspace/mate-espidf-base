#include "presentation/ble/handler/wifi_manager/dto.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

static void format_ipv4(const uint8_t ip[4], char* buf, size_t buf_cap) {
    snprintf(buf, buf_cap, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

size_t pres_ble_handler_wifi_manager_dto_encode_status(
    const dom_usecases_internal_wifi_manager_status_t* status,
    char*                                                buf,
    size_t                                                buf_cap
) {
    if (!status || !buf || buf_cap == 0) {
        return 0;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return 0;
    }

    cJSON_AddBoolToObject(root, "is_up", status->wifi.is_up);
    cJSON_AddStringToObject(
        root,
        "sta_connection_status",
        status->wifi.sta_connection_status == DOM_MODELS_WIFI_STA_STATUS_CONNECTED ? "connected" : "disconnected"
    );
    cJSON_AddStringToObject(root, "ssid", status->wifi.sta_ssid);

    char ip_str[16];
    format_ipv4(status->wifi.sta_ipv4, ip_str, sizeof(ip_str));
    cJSON_AddStringToObject(root, "ip", ip_str);

    char netmask_str[16];
    format_ipv4(status->wifi.sta_netmask, netmask_str, sizeof(netmask_str));
    cJSON_AddStringToObject(root, "netmask", netmask_str);

    char gateway_str[16];
    format_ipv4(status->wifi.sta_gateway, gateway_str, sizeof(gateway_str));
    cJSON_AddStringToObject(root, "gateway", gateway_str);

    cJSON_AddNumberToObject(root, "rssi", status->wifi.sta_rssi);
    cJSON_AddBoolToObject(root, "try_connect_on_init_enabled", status->try_connect_on_init_enabled);
    cJSON_AddBoolToObject(root, "connect_attempted", status->connect_attempted);

    bool ok = cJSON_PrintPreallocated(root, buf, (int)buf_cap, false);
    cJSON_Delete(root);

    return ok ? strnlen(buf, buf_cap) : 0;
}

size_t pres_ble_handler_wifi_manager_dto_encode_stored_credential(
    const dom_usecases_internal_wifi_manager_stored_sta_t* stored,
    char*                                                    buf,
    size_t                                                    buf_cap
) {
    if (!stored || !buf || buf_cap == 0) {
        return 0;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return 0;
    }

    cJSON_AddBoolToObject(root, "available", stored->available);
    cJSON_AddStringToObject(root, "ssid", stored->ssid);

    bool ok = cJSON_PrintPreallocated(root, buf, (int)buf_cap, false);
    cJSON_Delete(root);

    return ok ? strnlen(buf, buf_cap) : 0;
}

dom_models_error_t pres_ble_handler_wifi_manager_dto_decode_connect(
    const char*                            json,
    size_t                                  json_len,
    dom_models_wifi_sta_connect_config_t* out
) {
    if (!json || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));

    cJSON* root = cJSON_ParseWithLength(json, json_len);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* ssid_item     = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON* password_item = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (!cJSON_IsString(ssid_item) || !ssid_item->valuestring) {
        cJSON_Delete(root);
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    strncpy(out->ssid, ssid_item->valuestring, sizeof(out->ssid) - 1);
    if (cJSON_IsString(password_item) && password_item->valuestring) {
        strncpy(out->password, password_item->valuestring, sizeof(out->password) - 1);
    }

    cJSON_Delete(root);

    return DOMAIN_MODELS_ERROR_OK;
}
