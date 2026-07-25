#ifndef DOMAIN_CONTRACTS_SYSTEM_RESTART_H
#define DOMAIN_CONTRACTS_SYSTEM_RESTART_H

#include <stdint.h>
#include <stdlib.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_system_restart_t dom_contracts_system_restart_t;

struct dom_contracts_system_restart_t {
    void* ctx;
    dom_models_error_t (*restart)(
        dom_contracts_system_restart_t* self,
        uint32_t                        delay_ms
    );
};

static inline dom_contracts_system_restart_t* dom_contracts_system_restart_new(void* ctx) {
    dom_contracts_system_restart_t* self = (dom_contracts_system_restart_t*)calloc(1, sizeof(dom_contracts_system_restart_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_system_restart_delete(dom_contracts_system_restart_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_SYSTEM_RESTART_H */
