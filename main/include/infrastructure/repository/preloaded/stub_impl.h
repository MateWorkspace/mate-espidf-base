#ifndef INFRASTRUCTURE_REPOSITORY_PRELOADED_STUB_IMPL_H
#define INFRASTRUCTURE_REPOSITORY_PRELOADED_STUB_IMPL_H

#include "domain/contracts/repository/preloaded.h"
#include "infrastructure/repository/preloaded/stub_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_repository_preloaded_t* inf_repository_preloaded_stub_impl_new(const inf_repository_preloaded_stub_impl_cfg_t* cfg);

void inf_repository_preloaded_stub_impl_delete(dom_contracts_repository_preloaded_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_REPOSITORY_PRELOADED_STUB_IMPL_H */
