#ifndef COMPOSITION_COUNTER_TEST_UTILS_H
#define COMPOSITION_COUNTER_TEST_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

bool cmp_counter_test_utils_cstr_available(const char* value);

dom_models_error_t cmp_counter_test_utils_build_mqtt_client_id(char* out, size_t out_size);

dom_models_error_t cmp_counter_test_utils_build_mqtt_broker_uri(char* out, size_t out_size);

dom_models_error_t cmp_counter_test_utils_build_mqtt_lwt_topic(char* out, size_t out_size);

dom_models_error_t cmp_counter_test_utils_build_ble_device_name(char* out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITION_COUNTER_TEST_UTILS_H */
