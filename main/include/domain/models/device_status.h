#ifndef DOMAIN_MODELS_DEVICE_STATUS_H
#define DOMAIN_MODELS_DEVICE_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

#define DOM_MODELS_DEVICE_STATUS(X)                \
    X(DOM_MODELS_DEVICE_STATUS_OFFLINE, "OFFLINE") \
    X(DOM_MODELS_DEVICE_STATUS_ONLINE, "ONLINE")

typedef enum {
#define X(cb_name, cb_string) cb_name,
    DOM_MODELS_DEVICE_STATUS(X)
#undef X
} dom_models_device_status_t;

static inline const char* dom_models_device_status_str(dom_models_device_status_t status) {
    switch (status) {
#define X(cb_name, cb_string) \
    case cb_name:             \
        return cb_string;
        DOM_MODELS_DEVICE_STATUS(X)
#undef X
    }
    return "UNKNOWN";
}

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_DEVICE_STATUS_H */
