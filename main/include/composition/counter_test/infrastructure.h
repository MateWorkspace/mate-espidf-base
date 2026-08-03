#ifndef COMPOSITION_COUNTER_TEST_INFRASTRUCTURE_H
#define COMPOSITION_COUNTER_TEST_INFRASTRUCTURE_H

#include "composition/counter_test/types.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t cmp_counter_test_infrastructure_init(cmp_counter_test_launcher_t* launcher);

void cmp_counter_test_infrastructure_deinit(cmp_counter_test_launcher_t* launcher);

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITION_COUNTER_TEST_INFRASTRUCTURE_H */
