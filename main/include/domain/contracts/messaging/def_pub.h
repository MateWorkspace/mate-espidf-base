#ifndef DOMAIN_CONTRACTS_MESSAGING_DEF_PUB_H
#define DOMAIN_CONTRACTS_MESSAGING_DEF_PUB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "domain/models/device_status.h"
#include "domain/models/error.h"
#include "domain/models/preloaded.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dom_contracts_messaging_def_pub_t dom_contracts_messaging_def_pub_t;

struct dom_contracts_messaging_def_pub_t {
    void* ctx;
    dom_models_error_t (*is_connected)(
        dom_contracts_messaging_def_pub_t* self,
        bool*                              out
    );
    dom_models_error_t (*registration)(
        dom_contracts_messaging_def_pub_t* self,
        const char*                        device_id,
        const char*                        device_info,
        const char*                        node_class_name,
        const char*                        firmware_name,
        const dom_models_preloaded_kv_t*   config,
        size_t                             config_count
    );
    dom_models_error_t (*status)(
        dom_contracts_messaging_def_pub_t* self,
        const char*                        device_id,
        dom_models_device_status_t         status
    );
    dom_models_error_t (*log)(
        dom_contracts_messaging_def_pub_t* self,
        const char*                        device_id,
        const char*                        msg,
        size_t                             msg_len
    );
    dom_models_error_t (*action_ack)(
        dom_contracts_messaging_def_pub_t* self,
        const char*                        device_id,
        const char*                        execution_id,
        const char*                        status,
        const char*                        message
    );
    dom_models_error_t (*ir_capture)(
        dom_contracts_messaging_def_pub_t* self,
        const char*                        device_id,
        const int32_t*                     raw_data,
        size_t                             raw_data_count
    );
    dom_models_error_t (*ir_transmit_ack)(
        dom_contracts_messaging_def_pub_t* self,
        const char*                        device_id,
        const char*                        execution_id,
        const char*                        status,
        const char*                        message
    );
    dom_models_error_t (*telemetry)(
        dom_contracts_messaging_def_pub_t* self,
        const char*                        device_id,
        const char*                        metric_name,
        const char*                        payload_schema_name,
        int                                payload_schema_version,
        const char*                        payload_json
    );
};

static inline dom_contracts_messaging_def_pub_t* dom_contracts_messaging_def_pub_new(void* ctx) {
    dom_contracts_messaging_def_pub_t* self = (dom_contracts_messaging_def_pub_t*)calloc(1, sizeof(dom_contracts_messaging_def_pub_t));
    if (!self) {
        return NULL;
    }

    self->ctx = ctx;

    return self;
}

static inline void dom_contracts_messaging_def_pub_delete(dom_contracts_messaging_def_pub_t* self) {
    if (!self) {
        return;
    }

    self->ctx = NULL;
    free(self);
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CONTRACTS_MESSAGING_DEF_PUB_H */
