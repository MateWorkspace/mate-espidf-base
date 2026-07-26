#ifndef COMPOSITION_MAIN_PRESENTATION_H
#define COMPOSITION_MAIN_PRESENTATION_H

#include "composition/main/types.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t cmp_main_presentation_init(cmp_main_launcher_t* launcher);

void cmp_main_presentation_deinit(cmp_main_launcher_t* launcher);

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITION_MAIN_PRESENTATION_H */
