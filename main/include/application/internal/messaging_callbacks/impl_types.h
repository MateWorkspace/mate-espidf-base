#ifndef APPLICATION_INTERNAL_MESSAGING_CALLBACKS_IMPL_TYPES_H
#define APPLICATION_INTERNAL_MESSAGING_CALLBACKS_IMPL_TYPES_H

#include <stdbool.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/messaging/def_pub.h"
#include "domain/contracts/messaging/def_sub.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/system/info.h"
#include "domain/contracts/system/restart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dom_contracts_logger_leveled_t*       logger;
    dom_contracts_messaging_def_pub_t*    def_pub;
    dom_contracts_messaging_def_sub_t*    def_sub;
    dom_contracts_system_restart_t*       system_restart;
    dom_contracts_repository_preloaded_t* preloaded_repository;
    dom_contracts_system_info_t*          system_info;
} app_internal_messaging_callbacks_impl_cfg_t;

typedef struct {
    app_internal_messaging_callbacks_impl_cfg_t cfg;
    char                                         device_id_str[37];
    bool                                          log_cb_subscribed;
} app_internal_messaging_callbacks_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_INTERNAL_MESSAGING_CALLBACKS_IMPL_TYPES_H */
