#ifndef INFRASTRUCTURE_MESSAGING_DEF_SUB_MQTT_IMPL_H
#define INFRASTRUCTURE_MESSAGING_DEF_SUB_MQTT_IMPL_H

#include "domain/contracts/messaging/def_sub.h"
#include "infrastructure/messaging/def_sub/mqtt_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_messaging_def_sub_t* inf_messaging_def_sub_mqtt_impl_new(
    const inf_messaging_def_sub_mqtt_impl_cfg_t* cfg
);

void inf_messaging_def_sub_mqtt_impl_delete(dom_contracts_messaging_def_sub_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_MESSAGING_DEF_SUB_MQTT_IMPL_H */
