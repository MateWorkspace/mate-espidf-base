#ifndef COMPOSITION_INFRARED_INFRASTRUCTURE_H
#define COMPOSITION_INFRARED_INFRASTRUCTURE_H

#include "composition/infrared/types.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t cmp_infrared_infrastructure_init(cmp_infrared_launcher_t* launcher);

void cmp_infrared_infrastructure_deinit(cmp_infrared_launcher_t* launcher);

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITION_INFRARED_INFRASTRUCTURE_H */
