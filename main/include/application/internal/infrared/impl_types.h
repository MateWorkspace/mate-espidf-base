#ifndef APPLICATION_INTERNAL_INFRARED_IMPL_TYPES_H
#define APPLICATION_INTERNAL_INFRARED_IMPL_TYPES_H

#include <stdint.h>

#include "domain/contracts/device/ir.h"
#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/messaging/def_pub.h"
#include "domain/contracts/messaging/def_sub.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/models/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_INTERNAL_INFRARED_IMPL_RAW_DATA_MAX_LEN 512 /* matches INFRARED_RX_MAX_DURATIONS */

typedef struct {
    dom_contracts_logger_leveled_t*       logger;
    dom_contracts_device_ir_t*            ir;
    dom_contracts_messaging_def_pub_t*    def_pub;
    dom_contracts_messaging_def_sub_t*    def_sub;
    dom_contracts_repository_preloaded_t* preloaded_repository;
} app_internal_infrared_impl_cfg_t;

typedef struct {
    app_internal_infrared_impl_cfg_t cfg;
    char                             device_id_str[37];
    bool                             receive_handler_registered;
    /* Struct-owned (not stack-local) - transmit_impl and on_ir_receive run
       on the MQTT/RX worker task stacks respectively; a stack-local buffer
       of this size risks overflowing them (see presentation/mqtt/context.h's
       topic_scratch comment for the precedent this follows). Two separate
       fields since the element types differ (duration vs raw int32). Safe
       as struct-owned singletons: this usecase has one instance per
       composition and MQTT/IR message handling is inherently serial. */
    dom_models_ir_duration_t transmit_durations_scratch[APP_INTERNAL_INFRARED_IMPL_RAW_DATA_MAX_LEN];
    int32_t                  receive_raw_data_scratch[APP_INTERNAL_INFRARED_IMPL_RAW_DATA_MAX_LEN];
} app_internal_infrared_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_INFRARED_IMPL_TYPES_H */
