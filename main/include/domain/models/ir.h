#ifndef DOMAIN_MODELS_IR_H
#define DOMAIN_MODELS_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool     level;
    uint32_t duration_us;
} dom_models_ir_duration_t;

typedef void (*dom_models_ir_receive_cb_t)(
    const dom_models_ir_duration_t* durations,
    size_t                          duration_count,
    void*                           cb_ctx
);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_IR_H */
