#ifndef COMPOSITION_MAIN_UTILS_H
#define COMPOSITION_MAIN_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

bool cmp_main_utils_cstr_available(const char* value);

dom_models_error_t cmp_main_utils_build_mqtt_client_id(char* out, size_t out_size);

dom_models_error_t cmp_main_utils_build_mqtt_broker_uri(char* out, size_t out_size);

dom_models_error_t cmp_main_utils_build_mqtt_lwt_topic(char* out, size_t out_size);

dom_models_error_t cmp_main_utils_build_ble_device_name(char* out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITION_MAIN_UTILS_H */
