#ifndef DOMAIN_CONTRACTS_SYSTEM_INFO_H
#define DOMAIN_CONTRACTS_SYSTEM_INFO_H

#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_system_info_t dom_contracts_system_info_t;

struct dom_contracts_system_info_t {
    void* ctx;
    dom_models_error_t (*get_project_info)(
        dom_contracts_system_info_t*      self,
        dom_models_system_project_info_t* out
    );
    dom_models_error_t (*get_chip_info)(
        dom_contracts_system_info_t*   self,
        dom_models_system_chip_info_t* out
    );
};

static inline dom_contracts_system_info_t* dom_contracts_system_info_new(void* ctx) {
    dom_contracts_system_info_t* self = (dom_contracts_system_info_t*)calloc(1, sizeof(dom_contracts_system_info_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_system_info_delete(dom_contracts_system_info_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_SYSTEM_INFO_H */
