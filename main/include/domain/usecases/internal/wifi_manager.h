#ifndef DOMAIN_USECASES_INTERNAL_WIFI_MANAGER_H
#define DOMAIN_USECASES_INTERNAL_WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "domain/models/error.h"
#include "domain/models/wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_usecases_internal_wifi_manager_t             dom_usecases_internal_wifi_manager_t;
typedef struct dom_usecases_internal_wifi_manager_stored_sta_t  dom_usecases_internal_wifi_manager_stored_sta_t;
typedef struct dom_usecases_internal_wifi_manager_status_t      dom_usecases_internal_wifi_manager_status_t;

struct dom_usecases_internal_wifi_manager_t {
    void* ctx;
    dom_models_error_t (*start)(
        dom_usecases_internal_wifi_manager_t* self
    );
    dom_models_error_t (*stop)(
        dom_usecases_internal_wifi_manager_t* self
    );
    dom_models_error_t (*get_status)(
        dom_usecases_internal_wifi_manager_t*        self,
        dom_usecases_internal_wifi_manager_status_t* out
    );
    dom_models_error_t (*connect)(
        dom_usecases_internal_wifi_manager_t*       self,
        const dom_models_wifi_sta_connect_config_t* credential
    );
    dom_models_error_t (*connect_stored)(
        dom_usecases_internal_wifi_manager_t* self
    );
    dom_models_error_t (*disconnect)(
        dom_usecases_internal_wifi_manager_t* self
    );
    dom_models_error_t (*get_stored_credential)(
        dom_usecases_internal_wifi_manager_t*            self,
        dom_usecases_internal_wifi_manager_stored_sta_t* out
    );
    dom_models_error_t (*forget_stored_credential)(
        dom_usecases_internal_wifi_manager_t* self
    );
    dom_models_error_t (*get_try_connect_on_init)(
        dom_usecases_internal_wifi_manager_t* self,
        bool*                                 out
    );
    dom_models_error_t (*set_try_connect_on_init)(
        dom_usecases_internal_wifi_manager_t* self,
        bool                                  enabled
    );
    dom_models_error_t (*need_reconnect)(
        dom_usecases_internal_wifi_manager_t* self,
        bool*                                 out
    );
    dom_models_error_t (*try_reconnect)(
        dom_usecases_internal_wifi_manager_t* self,
        bool*                                 attempted
    );
};

struct dom_usecases_internal_wifi_manager_stored_sta_t {
    bool available;
    char ssid[32 + 1];
};

struct dom_usecases_internal_wifi_manager_status_t {
    dom_models_wifi_status_t                        wifi;
    dom_usecases_internal_wifi_manager_stored_sta_t stored;
    bool                                            try_connect_on_init_enabled;
    bool                                            connect_attempted;
};

static inline dom_usecases_internal_wifi_manager_t* dom_usecases_internal_wifi_manager_new(void* ctx) {
    dom_usecases_internal_wifi_manager_t* self = (dom_usecases_internal_wifi_manager_t*)calloc(1, sizeof(dom_usecases_internal_wifi_manager_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_wifi_manager_delete(dom_usecases_internal_wifi_manager_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_WIFI_MANAGER_H */
