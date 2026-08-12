#ifndef DOMAIN_CONTRACTS_DEVICE_IR_H
#define DOMAIN_CONTRACTS_DEVICE_IR_H

#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_device_ir_t dom_contracts_device_ir_t;

struct dom_contracts_device_ir_t {
    void* ctx;
    dom_models_error_t (*transmit)(
        dom_contracts_device_ir_t*      self,
        const dom_models_ir_duration_t* durations,
        size_t                          duration_count
    );
    dom_models_error_t (*set_receive_handler)(
        dom_contracts_device_ir_t* self,
        void*                      cb_ctx,
        dom_models_ir_receive_cb_t cb
    );
};

static inline dom_contracts_device_ir_t* dom_contracts_device_ir_new(void* ctx) {
    dom_contracts_device_ir_t* self = (dom_contracts_device_ir_t*)calloc(1, sizeof(dom_contracts_device_ir_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_device_ir_delete(dom_contracts_device_ir_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_DEVICE_IR_H */
