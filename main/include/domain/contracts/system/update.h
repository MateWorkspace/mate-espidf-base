#ifndef DOMAIN_CONTRACTS_SYSTEM_UPDATE_H
#define DOMAIN_CONTRACTS_SYSTEM_UPDATE_H

#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_system_update_t dom_contracts_system_update_t;

struct dom_contracts_system_update_t {
    void* ctx;
    dom_models_error_t (*update)(
        dom_contracts_system_update_t*  self,
        const dom_models_update_info_t* update_info
    );
    dom_models_error_t (*validate)(
        dom_contracts_system_update_t* self
    );
    dom_models_error_t (*rollback)(
        dom_contracts_system_update_t* self
    );
};

static inline dom_contracts_system_update_t* dom_contracts_system_update_new(void* ctx) {
    dom_contracts_system_update_t* self = (dom_contracts_system_update_t*)calloc(1, sizeof(dom_contracts_system_update_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_system_update_delete(dom_contracts_system_update_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_SYSTEM_UPDATE_H */
