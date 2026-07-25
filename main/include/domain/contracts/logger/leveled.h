#ifndef DOMAIN_CONTRACTS_LOGGER_LEVELED_H
#define DOMAIN_CONTRACTS_LOGGER_LEVELED_H

#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_logger_leveled_t dom_contracts_logger_leveled_t;

typedef void (*dom_contracts_logger_leveled_cb)(void* cb_ctx, const char* msg, size_t msg_len);

struct dom_contracts_logger_leveled_t {
    void* ctx;
    void (*error)(
        dom_contracts_logger_leveled_t* self,
        const char*                     tag,
        const char*                     format,
        ...
    );
    void (*warn)(
        dom_contracts_logger_leveled_t* self,
        const char*                     tag,
        const char*                     format,
        ...
    );
    void (*info)(
        dom_contracts_logger_leveled_t* self,
        const char*                     tag,
        const char*                     format,
        ...
    );
    void (*debug)(
        dom_contracts_logger_leveled_t* self,
        const char*                     tag,
        const char*                     format,
        ...
    );
    void (*add_callback)(
        dom_contracts_logger_leveled_t* self,
        void*                           cb_ctx,
        dom_contracts_logger_leveled_cb cb_func
    );
    void (*remove_callback)(
        dom_contracts_logger_leveled_t* self,
        dom_contracts_logger_leveled_cb cb_func
    );
};

static inline dom_contracts_logger_leveled_t* dom_contracts_logger_leveled_new(void* ctx) {
    dom_contracts_logger_leveled_t* self = (dom_contracts_logger_leveled_t*)calloc(1, sizeof(dom_contracts_logger_leveled_t));
    if (!self) {
        return NULL;
    }
    self->ctx = ctx;
    return self;
}

static inline void dom_contracts_logger_leveled_delete(dom_contracts_logger_leveled_t* self) {
    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_LOGGER_LEVELED_H */
