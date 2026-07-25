#ifndef INFRASTRUCTURE_REPOSITORY_PRELOADED_STUB_IMPL_UTILS_H
#define INFRASTRUCTURE_REPOSITORY_PRELOADED_STUB_IMPL_UTILS_H

#include <stddef.h>

#include "domain/models/error.h"
#include "infrastructure/repository/preloaded/stub_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_repository_preloaded_stub_impl_copy_cstr(char* out, size_t out_size, const char* value);

dom_models_error_t inf_repository_preloaded_stub_impl_set_string(char** runtime_value, const char* value);

dom_models_error_t inf_repository_preloaded_stub_impl_load_cfg(inf_repository_preloaded_stub_impl_ctx_t* ctx, const inf_repository_preloaded_stub_impl_cfg_t* cfg);

void inf_repository_preloaded_stub_impl_clear(inf_repository_preloaded_stub_impl_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_REPOSITORY_PRELOADED_STUB_IMPL_UTILS_H */
