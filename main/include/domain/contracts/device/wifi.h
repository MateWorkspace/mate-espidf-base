#ifndef DOMAIN_CONTRACTS_DEVICE_WIFI_H
#define DOMAIN_CONTRACTS_DEVICE_WIFI_H

#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_device_wifi_t dom_contracts_device_wifi_t;

struct dom_contracts_device_wifi_t {
    void* ctx;
    dom_models_error_t (*start)(
        dom_contracts_device_wifi_t* self
    );
    dom_models_error_t (*stop)(
        dom_contracts_device_wifi_t* self
    );
    dom_models_error_t (*get_status)(
        dom_contracts_device_wifi_t* self,
        dom_models_wifi_status_t*    out
    );
    dom_models_error_t (*connect_sta)(
        dom_contracts_device_wifi_t*                self,
        const dom_models_wifi_sta_connect_config_t* config
    );
    dom_models_error_t (*disconnect_sta)(
        dom_contracts_device_wifi_t* self
    );
    dom_models_error_t (*add_event_callback)(
        dom_contracts_device_wifi_t*     self,
        void*                            cb_ctx,
        dom_models_wifi_event_callback_t cb_func
    );
    dom_models_error_t (*remove_event_callback)(
        dom_contracts_device_wifi_t*     self,
        dom_models_wifi_event_callback_t cb_func
    );
};

static inline dom_contracts_device_wifi_t* dom_contracts_device_wifi_new(void* ctx) {
    dom_contracts_device_wifi_t* self = (dom_contracts_device_wifi_t*)calloc(1, sizeof(dom_contracts_device_wifi_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_device_wifi_delete(dom_contracts_device_wifi_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_DEVICE_WIFI_H */
