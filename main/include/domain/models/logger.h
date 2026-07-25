#ifndef DOMAIN_MODELS_LOGGER_H
#define DOMAIN_MODELS_LOGGER_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DOMAIN_MODELS_LOGGER_LEVEL(X)            \
    X(DOMAIN_MODELS_LOGGER_LEVEL_NONE, "NONE")   \
    X(DOMAIN_MODELS_LOGGER_LEVEL_ERROR, "ERROR") \
    X(DOMAIN_MODELS_LOGGER_LEVEL_WARN, "WARN")   \
    X(DOMAIN_MODELS_LOGGER_LEVEL_INFO, "INFO")   \
    X(DOMAIN_MODELS_LOGGER_LEVEL_DEBUG, "DEBUG")

typedef enum {
#define X(cb_name, cb_string) cb_name,
    DOMAIN_MODELS_LOGGER_LEVEL(X)
#undef X
} dom_models_logger_level_t;

static inline const char* dom_models_logger_level_str(dom_models_logger_level_t log_level) {
    switch (log_level) {
#define X(cb_name, cb_string) \
    case cb_name:             \
        return cb_string;
        DOMAIN_MODELS_LOGGER_LEVEL(X)
#undef X
    }
    return "NONE";
}

static inline dom_models_logger_level_t dom_models_logger_level_from_str(const char* str) {
    if (!str) {
        return DOMAIN_MODELS_LOGGER_LEVEL_NONE;
    }

#define X(cb_name, cb_string)          \
    if (strcmp(str, cb_string) == 0) { \
        return cb_name;                \
    }
    DOMAIN_MODELS_LOGGER_LEVEL(X)
#undef X

    return DOMAIN_MODELS_LOGGER_LEVEL_NONE;
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_LOGGER_H */
