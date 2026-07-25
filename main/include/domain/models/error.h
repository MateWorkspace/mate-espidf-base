#ifndef DOMAIN_MODELS_ERROR_H
#define DOMAIN_MODELS_ERROR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DOMAIN_MODELS_ERROR(X)                              \
    X(DOMAIN_MODELS_ERROR_OK, "OK")                         \
    X(DOMAIN_MODELS_ERROR_UNIMPLEMENTED, "UNIMPLEMENTED")   \
    X(DOMAIN_MODELS_ERROR_NOT_SUPPORTED, "NOT_SUPPORTED")   \
    X(DOMAIN_MODELS_ERROR_NOT_FOUND, "NOT_FOUND")           \
    X(DOMAIN_MODELS_ERROR_MALLOC_FAILED, "MALLOC_FAILED")   \
    X(DOMAIN_MODELS_ERROR_BAD_ARGUMENT, "INVALID_ARGUMENT") \
    X(DOMAIN_MODELS_ERROR_BAD_STATE, "BAD_STATE")           \
    X(DOMAIN_MODELS_ERROR_TIMEOUT, "TIMEOUT")               \
    X(DOMAIN_MODELS_ERROR_FAILURE, "FAILURE")               \
    X(DOMAIN_MODELS_ERROR_UNKNOWN, "UNKNOWN")

typedef enum {
#define X(cb_name, cb_string) cb_name,
    DOMAIN_MODELS_ERROR(X)
#undef X
} dom_models_error_t;

static inline const char* dom_models_error_str(dom_models_error_t err) {
    switch (err) {
#define X(cb_name, cb_string) \
    case cb_name:             \
        return cb_string;
        DOMAIN_MODELS_ERROR(X)
#undef X
    }
    return "UNKNOWN";
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_ERROR_H */
