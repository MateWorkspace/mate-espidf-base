#include "composition/counter_test/utils.h"

#include <stdio.h>

#include "domain/models/preloaded.h"

bool cmp_counter_test_utils_cstr_available(const char* value) {
    return value && value[0] != '\0';
}

dom_models_error_t cmp_counter_test_utils_build_mqtt_client_id(char* out, size_t out_size) {
    if (!out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int written = snprintf(out, out_size, "%s-%s", PROJECT_NAME, dom_models_preloaded_data.device_id_str);
    if (written <= 0 || (size_t)written >= out_size) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t cmp_counter_test_utils_build_mqtt_broker_uri(char* out, size_t out_size) {
    if (!out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int written = snprintf(
        out,
        out_size,
        "%s://%s:%s",
        dom_models_preloaded_data.mqtt_proto,
        dom_models_preloaded_data.mqtt_host,
        dom_models_preloaded_data.mqtt_port
    );
    if (written <= 0 || (size_t)written >= out_size) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t cmp_counter_test_utils_build_ble_device_name(char* out, size_t out_size) {
    if (!out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int written = snprintf(out, out_size, "matedev_%s", dom_models_preloaded_data.device_id_str);
    if (written <= 0 || (size_t)written >= out_size) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t cmp_counter_test_utils_build_mqtt_lwt_topic(char* out, size_t out_size) {
    if (!out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int written = snprintf(out, out_size, "/pub/%s/status", dom_models_preloaded_data.device_id_str);
    if (written <= 0 || (size_t)written >= out_size) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
