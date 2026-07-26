#ifndef DOMAIN_CONTRACTS_MESSAGING_DEF_SUB_H
#define DOMAIN_CONTRACTS_MESSAGING_DEF_SUB_H

#include <stdlib.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_messaging_def_sub_t dom_contracts_messaging_def_sub_t;

struct dom_contracts_messaging_def_sub_t {
    void* ctx;
    dom_models_error_t (*registration_ack)(
        const char*                        device_id,
        dom_contracts_messaging_def_sub_t* self
    );
    dom_models_error_t (*ota)(
        const char*                        device_id,
        dom_contracts_messaging_def_sub_t* self
    );
    dom_models_error_t (*action)(
        const char*                        device_id,
        dom_contracts_messaging_def_sub_t* self
    );
};

static inline dom_contracts_messaging_def_sub_t* dom_contracts_messaging_def_sub_new(void* ctx) {
    dom_contracts_messaging_def_sub_t* self = (dom_contracts_messaging_def_sub_t*)calloc(1, sizeof(dom_contracts_messaging_def_sub_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_messaging_def_sub_delete(dom_contracts_messaging_def_sub_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_MESSAGING_DEF_SUB_H */
