#ifndef DOMAIN_CONTRACTS_REPOSITORY_PRELOADED_H
#define DOMAIN_CONTRACTS_REPOSITORY_PRELOADED_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_repository_preloaded_t dom_contracts_repository_preloaded_t;

struct dom_contracts_repository_preloaded_t {
    void* ctx;
    dom_models_error_t (*get_device_id)(
        dom_contracts_repository_preloaded_t* self,
        uint64_t*                             out
    );
    dom_models_error_t (*get_device_id_str)(
        dom_contracts_repository_preloaded_t* self,
        char*                                 out,
        size_t                                out_size
    );
    dom_models_error_t (*get_wifi_ap_pass)(
        dom_contracts_repository_preloaded_t* self,
        char*                                 out,
        size_t                                out_size
    );
    dom_models_error_t (*set_wifi_ap_pass)(
        dom_contracts_repository_preloaded_t* self,
        const char*                           value
    );
    dom_models_error_t (*get_mqtt_proto)(
        dom_contracts_repository_preloaded_t* self,
        char*                                 out,
        size_t                                out_size
    );
    dom_models_error_t (*set_mqtt_proto)(
        dom_contracts_repository_preloaded_t* self,
        const char*                           value
    );
    dom_models_error_t (*get_mqtt_host)(
        dom_contracts_repository_preloaded_t* self,
        char*                                 out,
        size_t                                out_size
    );
    dom_models_error_t (*set_mqtt_host)(
        dom_contracts_repository_preloaded_t* self,
        const char*                           value
    );
    dom_models_error_t (*get_mqtt_port)(
        dom_contracts_repository_preloaded_t* self,
        char*                                 out,
        size_t                                out_size
    );
    dom_models_error_t (*set_mqtt_port)(
        dom_contracts_repository_preloaded_t* self,
        const char*                           value
    );
    dom_models_error_t (*get_mqtt_user)(
        dom_contracts_repository_preloaded_t* self,
        char*                                 out,
        size_t                                out_size
    );
    dom_models_error_t (*set_mqtt_user)(
        dom_contracts_repository_preloaded_t* self,
        const char*                           value
    );
    dom_models_error_t (*get_mqtt_pass)(
        dom_contracts_repository_preloaded_t* self,
        char*                                 out,
        size_t                                out_size
    );
    dom_models_error_t (*set_mqtt_pass)(
        dom_contracts_repository_preloaded_t* self,
        const char*                           value
    );
    dom_models_error_t (*get_system_restart_after_ms)(
        dom_contracts_repository_preloaded_t* self,
        uint32_t*                             out
    );
    dom_models_error_t (*set_system_restart_after_ms)(
        dom_contracts_repository_preloaded_t* self,
        uint32_t                              value
    );
};

static inline dom_contracts_repository_preloaded_t* dom_contracts_repository_preloaded_new(void* ctx) {
    dom_contracts_repository_preloaded_t* self = (dom_contracts_repository_preloaded_t*)calloc(1, sizeof(dom_contracts_repository_preloaded_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_repository_preloaded_delete(dom_contracts_repository_preloaded_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_REPOSITORY_PRELOADED_H */
