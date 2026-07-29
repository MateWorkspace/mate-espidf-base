#ifndef DOMAIN_USECASES_INTERNAL_SETTINGS_H
#define DOMAIN_USECASES_INTERNAL_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_usecases_internal_settings_t                  dom_usecases_internal_settings_t;
typedef struct dom_usecases_internal_settings_snapshot_t         dom_usecases_internal_settings_snapshot_t;
typedef struct dom_usecases_internal_settings_preloaded_update_t dom_usecases_internal_settings_preloaded_update_t;

struct dom_usecases_internal_settings_t {
    void* ctx;
    dom_models_error_t (*get_snapshot)(
        dom_usecases_internal_settings_t*          self,
        dom_usecases_internal_settings_snapshot_t* out
    );
    dom_models_error_t (*set_preloaded)(
        dom_usecases_internal_settings_t*                        self,
        const dom_usecases_internal_settings_preloaded_update_t* update,
        bool*                                                    restart_required_out
    );
    dom_models_error_t (*get_restart_required)(
        dom_usecases_internal_settings_t* self,
        bool*                              out
    );
    dom_models_error_t (*restart)(
        dom_usecases_internal_settings_t* self,
        uint32_t                          delay_ms
    );
};

struct dom_usecases_internal_settings_snapshot_t {
    uint64_t device_id;
    char     device_id_str[32];

    char mqtt_proto[16];
    char mqtt_host[128];
    char mqtt_port[8];
    char mqtt_user[64];
    char mqtt_pass[128];

    uint32_t system_restart_after_ms;
};

struct dom_usecases_internal_settings_preloaded_update_t {
    bool mqtt_proto_set;
    char mqtt_proto[16];

    bool mqtt_host_set;
    char mqtt_host[128];

    bool mqtt_port_set;
    char mqtt_port[8];

    bool mqtt_user_set;
    char mqtt_user[64];

    bool mqtt_pass_set;
    char mqtt_pass[128];

    bool     system_restart_after_ms_set;
    uint32_t system_restart_after_ms;
};

static inline dom_usecases_internal_settings_t* dom_usecases_internal_settings_new(void* ctx) {
    dom_usecases_internal_settings_t* self = (dom_usecases_internal_settings_t*)calloc(1, sizeof(dom_usecases_internal_settings_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_usecases_internal_settings_delete(dom_usecases_internal_settings_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_USECASES_INTERNAL_SETTINGS_H */
