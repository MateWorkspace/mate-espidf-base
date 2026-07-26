#ifndef DOMAIN_MODELS_UPDATE_H
#define DOMAIN_MODELS_UPDATE_H

#include <stddef.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DOM_MODELS_UPDATE_URL_MAX_LEN      256
#define DOM_MODELS_UPDATE_CHECKSUM_MAX_LEN 65

typedef struct {
    char   firmware_url[256];
    size_t firmware_size;
    char   firmware_checksum[65];
} dom_models_update_info_t;

typedef enum {
    DOM_MODELS_UPDATE_EVENT_COMPLETED = 0,
    DOM_MODELS_UPDATE_EVENT_PROGRESS,
} dom_models_update_event_type_t;

typedef struct {
    dom_models_update_event_type_t type;
    dom_models_error_t             result;        /* valid when type == COMPLETED */
    size_t                         bytes_written; /* valid when type == PROGRESS */
    size_t                         total_bytes;   /* valid when type == PROGRESS */
} dom_models_update_event_t;

typedef void (*dom_models_update_event_callback_t)(void* cb_ctx, const dom_models_update_event_t* event);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_UPDATE_H */
