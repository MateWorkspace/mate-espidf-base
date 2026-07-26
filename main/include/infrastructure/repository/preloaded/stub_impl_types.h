#ifndef INFRASTRUCTURE_REPOSITORY_PRELOADED_STUB_IMPL_TYPES_H
#define INFRASTRUCTURE_REPOSITORY_PRELOADED_STUB_IMPL_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t    device_id;
    const char* device_id_str;
    const char* mqtt_proto;
    const char* mqtt_host;
    const char* mqtt_port;
    const char* mqtt_user;
    const char* mqtt_pass;
    uint32_t    system_restart_after_ms;
    bool        wifi_sta_try_connect_on_init;
} inf_repository_preloaded_stub_impl_cfg_t;

#define INF_REPOSITORY_PRELOADED_STUB_IMPL_CFG_DEFAULT()      \
    {                                                         \
        .device_id                    = 0x020000000001ULL,    \
        .device_id_str                = "020000000001",       \
        .mqtt_proto                   = "mqtt",               \
        .mqtt_host                    = "127.0.0.1",          \
        .mqtt_port                    = "1883",               \
        .mqtt_user                    = "",                   \
        .mqtt_pass                    = "",                   \
        .system_restart_after_ms      = 0,                    \
        .wifi_sta_try_connect_on_init = false,                \
    }

typedef struct {
    uint64_t device_id;
    char*    device_id_str;
    char*    mqtt_proto;
    char*    mqtt_host;
    char*    mqtt_port;
    char*    mqtt_user;
    char*    mqtt_pass;
    uint32_t system_restart_after_ms;
    bool     wifi_sta_try_connect_on_init;
} inf_repository_preloaded_stub_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_REPOSITORY_PRELOADED_STUB_IMPL_TYPES_H */
