#ifndef DOMAIN_MODELS_UPDATE_H
#define DOMAIN_MODELS_UPDATE_H

#include <stddef.h>

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

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_UPDATE_H */
