#ifndef COMPOSITION_COUNTER_TEST_PRELOADED_H
#define COMPOSITION_COUNTER_TEST_PRELOADED_H

#include "domain/models/error.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t cmp_counter_test_preloaded_load_from_nvs(nvs_handle_t nvs);

void cmp_counter_test_preloaded_free();

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITION_COUNTER_TEST_PRELOADED_H */
