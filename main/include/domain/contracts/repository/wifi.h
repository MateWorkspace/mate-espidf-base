#ifndef DOMAIN_CONTRACTS_REPOSITORY_WIFI_H
#define DOMAIN_CONTRACTS_REPOSITORY_WIFI_H

#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_repository_wifi_t dom_contracts_repository_wifi_t;

struct dom_contracts_repository_wifi_t {
    void* ctx;
    dom_models_error_t (*get_sta_credential)(
        dom_contracts_repository_wifi_t*      self,
        dom_models_wifi_sta_connect_config_t* out
    );
    dom_models_error_t (*set_sta_credential)(
        dom_contracts_repository_wifi_t*            self,
        const dom_models_wifi_sta_connect_config_t* credential
    );
    dom_models_error_t (*clear_sta_credential)(
        dom_contracts_repository_wifi_t* self
    );
};

static inline dom_contracts_repository_wifi_t* dom_contracts_repository_wifi_new(void* ctx) {
    dom_contracts_repository_wifi_t* self = (dom_contracts_repository_wifi_t*)calloc(1, sizeof(dom_contracts_repository_wifi_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_repository_wifi_delete(dom_contracts_repository_wifi_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_REPOSITORY_WIFI_H */
