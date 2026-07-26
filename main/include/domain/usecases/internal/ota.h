#ifndef DOMAIN_USECASES_INTERNAL_OTA_H
#define DOMAIN_USECASES_INTERNAL_OTA_H

#include <stdbool.h>
#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_usecases_internal_ota_t        dom_usecases_internal_ota_t;
typedef struct dom_usecases_internal_ota_status_t dom_usecases_internal_ota_status_t;

struct dom_usecases_internal_ota_t {
    void* ctx;
    dom_models_error_t (*update)(
        dom_usecases_internal_ota_t*    self,
        const dom_models_update_info_t* update_info
    );
    dom_models_error_t (*validate)(
        dom_usecases_internal_ota_t* self
    );
    dom_models_error_t (*rollback)(
        dom_usecases_internal_ota_t* self
    );
    dom_models_error_t (*get_status)(
        dom_usecases_internal_ota_t*        self,
        dom_usecases_internal_ota_status_t* out
    );
};

struct dom_usecases_internal_ota_status_t {
    bool               updating;
    dom_models_error_t last_result;
};

static inline dom_usecases_internal_ota_t* dom_usecases_internal_ota_new(void* ctx) {
    dom_usecases_internal_ota_t* self = (dom_usecases_internal_ota_t*)calloc(1, sizeof(dom_usecases_internal_ota_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_ota_delete(dom_usecases_internal_ota_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_OTA_H */
