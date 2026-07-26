#ifndef DOMAIN_USECASES_INTERNAL_MESSAGING_CALLBACKS_H
#define DOMAIN_USECASES_INTERNAL_MESSAGING_CALLBACKS_H

#include <stdint.h>
#include <stdlib.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_usecases_internal_messaging_callbacks_t dom_usecases_internal_messaging_callbacks_t;

struct dom_usecases_internal_messaging_callbacks_t {
    void* ctx;
    dom_models_error_t (*publish_registration)(
        dom_usecases_internal_messaging_callbacks_t* self
    );
    dom_models_error_t (*publish_online_status)(
        dom_usecases_internal_messaging_callbacks_t* self
    );
    dom_models_error_t (*subscribe_defaults)(
        dom_usecases_internal_messaging_callbacks_t* self
    );
    dom_models_error_t (*restart)(
        dom_usecases_internal_messaging_callbacks_t* self,
        uint32_t                                     delay_ms
    );
};

static inline dom_usecases_internal_messaging_callbacks_t* dom_usecases_internal_messaging_callbacks_new(void* ctx) {
    dom_usecases_internal_messaging_callbacks_t* self = (dom_usecases_internal_messaging_callbacks_t*)calloc(1, sizeof(dom_usecases_internal_messaging_callbacks_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_messaging_callbacks_delete(dom_usecases_internal_messaging_callbacks_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_MESSAGING_CALLBACKS_H */
