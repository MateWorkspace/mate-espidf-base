#ifndef APPLICATION_INTERNAL_INFRARED_IMPL_TYPES_H
#define APPLICATION_INTERNAL_INFRARED_IMPL_TYPES_H

#include "domain/contracts/device/ir.h"
#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/messaging/def_pub.h"
#include "domain/contracts/messaging/def_sub.h"
#include "domain/contracts/repository/preloaded.h"

#ifdef __cplusplus
extern "C" {
#endif

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
} app_internal_infrared_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_INFRARED_IMPL_TYPES_H */
