#ifndef INFRASTRUCTURE_MESSAGING_DEF_PUB_STUB_IMPL_H
#define INFRASTRUCTURE_MESSAGING_DEF_PUB_STUB_IMPL_H

#include "domain/contracts/messaging/def_pub.h"
#include "infrastructure/messaging/def_pub/stub_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_messaging_def_pub_t* inf_messaging_def_pub_stub_impl_new(
    const inf_messaging_def_pub_stub_impl_cfg_t* cfg
);

void inf_messaging_def_pub_stub_impl_delete(dom_contracts_messaging_def_pub_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_MESSAGING_DEF_PUB_STUB_IMPL_H */
