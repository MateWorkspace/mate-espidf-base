#ifndef DOMAIN_USECASES_INTERNAL_SYSTEM_INFO_H
#define DOMAIN_USECASES_INTERNAL_SYSTEM_INFO_H

#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/preloaded.h"
#include "domain/models/system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_usecases_internal_system_info_t dom_usecases_internal_system_info_t;

struct dom_usecases_internal_system_info_t {
    void* ctx;
    dom_models_error_t (*get_project_info)(
        dom_usecases_internal_system_info_t* self,
        dom_models_system_project_info_t*    out
    );
    dom_models_error_t (*get_chip_info)(
        dom_usecases_internal_system_info_t* self,
        dom_models_system_chip_info_t*       out
    );
    dom_models_error_t (*get_preloaded_schema)(
        dom_usecases_internal_system_info_t*        self,
        const dom_models_preloaded_schema_entry_t** out,
        size_t*                                     out_count
    );
};

static inline dom_usecases_internal_system_info_t* dom_usecases_internal_system_info_new(void* ctx) {
    dom_usecases_internal_system_info_t* self = (dom_usecases_internal_system_info_t*)calloc(1, sizeof(dom_usecases_internal_system_info_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_system_info_delete(dom_usecases_internal_system_info_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_SYSTEM_INFO_H */
