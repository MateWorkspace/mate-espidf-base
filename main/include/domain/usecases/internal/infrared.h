#ifndef DOMAIN_USECASES_INTERNAL_INFRARED_H
#define DOMAIN_USECASES_INTERNAL_INFRARED_H

#include <stdint.h>
#include <stdlib.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_usecases_internal_infrared_t dom_usecases_internal_infrared_t;

struct dom_usecases_internal_infrared_t {
    void* ctx;
    dom_models_error_t (*subscribe)(
        dom_usecases_internal_infrared_t* self
    );
    dom_models_error_t (*transmit)(
        dom_usecases_internal_infrared_t* self,
        const char*                       execution_id,
        const int32_t*                    raw_data,
        size_t                            raw_data_count
    );
};

static inline dom_usecases_internal_infrared_t* dom_usecases_internal_infrared_new(void* ctx) {
    dom_usecases_internal_infrared_t* self = (dom_usecases_internal_infrared_t*)calloc(1, sizeof(dom_usecases_internal_infrared_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_infrared_delete(dom_usecases_internal_infrared_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_INFRARED_H */
