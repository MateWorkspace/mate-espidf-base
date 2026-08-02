#include "presentation/ble/handler/settings/dto.h"

#include <string.h>

#include "cJSON.h"

size_t pres_ble_handler_settings_dto_encode_snapshot(
    const dom_usecases_internal_settings_snapshot_t* snapshot,
    char*                                            buf,
    size_t                                           buf_cap
) {
    if (!snapshot || !buf || buf_cap == 0) {
        return 0;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return 0;
    }

    cJSON_AddStringToObject(root, "device_id_str", snapshot->device_id_str);
    cJSON_AddStringToObject(root, "mqtt_proto", snapshot->mqtt_proto);
    cJSON_AddStringToObject(root, "mqtt_host", snapshot->mqtt_host);
    cJSON_AddStringToObject(root, "mqtt_port", snapshot->mqtt_port);
    cJSON_AddStringToObject(root, "mqtt_user", snapshot->mqtt_user);
    cJSON_AddBoolToObject(root, "mqtt_pass_set", snapshot->mqtt_pass[0] != '\0');
    cJSON_AddNumberToObject(root, "system_restart_after_ms", snapshot->system_restart_after_ms);
    cJSON_AddBoolToObject(root, "wifi_try_init", snapshot->wifi_sta_try_connect_on_init);

    bool ok = cJSON_PrintPreallocated(root, buf, (int)buf_cap, false);
    cJSON_Delete(root);

    return ok ? strnlen(buf, buf_cap) : 0;
}

size_t pres_ble_handler_settings_dto_encode_restart_required(bool restart_required, char* buf, size_t buf_cap) {
    if (!buf || buf_cap == 0) {
        return 0;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return 0;
    }

    cJSON_AddBoolToObject(root, "restart_required", restart_required);

    bool ok = cJSON_PrintPreallocated(root, buf, (int)buf_cap, false);
    cJSON_Delete(root);

    return ok ? strnlen(buf, buf_cap) : 0;
}

static void copy_field_if_present(cJSON* root, const char* key, char* dst, size_t dst_cap, bool* set_flag) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return;
    }

    strncpy(dst, item->valuestring, dst_cap - 1);
    dst[dst_cap - 1] = '\0';
    *set_flag        = true;
}

dom_models_error_t pres_ble_handler_settings_dto_decode_update(
    const char*                                        json,
    size_t                                             json_len,
    dom_usecases_internal_settings_preloaded_update_t* out
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

    copy_field_if_present(root, "mqtt_proto", out->mqtt_proto, sizeof(out->mqtt_proto), &out->mqtt_proto_set);
    copy_field_if_present(root, "mqtt_host", out->mqtt_host, sizeof(out->mqtt_host), &out->mqtt_host_set);
    copy_field_if_present(root, "mqtt_port", out->mqtt_port, sizeof(out->mqtt_port), &out->mqtt_port_set);
    copy_field_if_present(root, "mqtt_user", out->mqtt_user, sizeof(out->mqtt_user), &out->mqtt_user_set);
    copy_field_if_present(root, "mqtt_pass", out->mqtt_pass, sizeof(out->mqtt_pass), &out->mqtt_pass_set);

    cJSON* restart_after_ms_item = cJSON_GetObjectItemCaseSensitive(root, "system_restart_after_ms");
    if (cJSON_IsNumber(restart_after_ms_item)) {
        out->system_restart_after_ms     = (uint32_t)restart_after_ms_item->valuedouble;
        out->system_restart_after_ms_set = true;
    }

    cJSON* wifi_try_init_item = cJSON_GetObjectItemCaseSensitive(root, "wifi_try_init");
    if (cJSON_IsBool(wifi_try_init_item)) {
        out->wifi_try_init     = cJSON_IsTrue(wifi_try_init_item);
        out->wifi_try_init_set = true;
    }

    cJSON_Delete(root);

    return DOMAIN_MODELS_ERROR_OK;
}
