#ifndef INFRASTRUCTURE_SYSTEM_RESTART_ESP_IMPL_TYPES_H
#define INFRASTRUCTURE_SYSTEM_RESTART_ESP_IMPL_TYPES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool reserved;
} inf_system_restart_esp_impl_cfg_t;

#define INF_SYSTEM_RESTART_ESP_IMPL_CFG_DEFAULT() \
    {                                             \
        .reserved = false,                        \
    }

typedef struct {
    inf_system_restart_esp_impl_cfg_t cfg;
} inf_system_restart_esp_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_RESTART_ESP_IMPL_TYPES_H */
