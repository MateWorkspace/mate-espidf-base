#ifndef DOMAIN_USECASES_INTERNAL_LOG_FORWARDING_H
#define DOMAIN_USECASES_INTERNAL_LOG_FORWARDING_H

#include <stdlib.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_usecases_internal_log_forwarding_t dom_usecases_internal_log_forwarding_t;

/* Fans out every logger line to whichever transports (MQTT, BLE, ...) have
   registered a sink - the single point through which any transport gets
   log lines, so the layering rule is the same for every transport instead
   of each one deciding independently whether to subscribe to
   dom_contracts_logger_leveled_t directly. */
struct dom_usecases_internal_log_forwarding_t {
    void* ctx;
    dom_models_error_t (*add_sink)(
        dom_usecases_internal_log_forwarding_t* self,
        void*                                   cb_ctx,
        dom_contracts_logger_leveled_cb         cb_func
    );
    dom_models_error_t (*remove_sink)(
        dom_usecases_internal_log_forwarding_t* self,
        dom_contracts_logger_leveled_cb         cb_func
    );
};

static inline dom_usecases_internal_log_forwarding_t* dom_usecases_internal_log_forwarding_new(void* ctx) {
    dom_usecases_internal_log_forwarding_t* self = (dom_usecases_internal_log_forwarding_t*)calloc(1, sizeof(dom_usecases_internal_log_forwarding_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_log_forwarding_delete(dom_usecases_internal_log_forwarding_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_LOG_FORWARDING_H */
