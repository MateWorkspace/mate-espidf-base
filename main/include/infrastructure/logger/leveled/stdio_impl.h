#ifndef INFRASTRUCTURE_LOGGER_LEVELED_STDIO_IMPL_H
#define INFRASTRUCTURE_LOGGER_LEVELED_STDIO_IMPL_H

#include <stddef.h>

#include "domain/contracts/logger/leveled.h"
#include "infrastructure/logger/leveled/stdio_impl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_contracts_logger_leveled_t* inf_logger_leveled_stdio_impl_new(const inf_logger_leveled_stdio_impl_cfg_t* cfg);

void inf_logger_leveled_stdio_impl_delete(dom_contracts_logger_leveled_t* self);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_LOGGING_LEVELED_STDIO_IMPL_H */