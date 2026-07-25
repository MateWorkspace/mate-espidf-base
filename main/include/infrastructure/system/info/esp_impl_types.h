#ifndef INFRASTRUCTURE_SYSTEM_INFO_ESP_IMPL_TYPES_H
#define INFRASTRUCTURE_SYSTEM_INFO_ESP_IMPL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_PROJECT_NAME
#ifdef PROJECT_NAME
#define INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_PROJECT_NAME PROJECT_NAME
#else
#define INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_PROJECT_NAME "unknown"
#endif
#endif

#ifndef INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_PROJECT_VERSION
#ifdef PROJECT_VER
#define INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_PROJECT_VERSION PROJECT_VER
#else
#define INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_PROJECT_VERSION "unknown"
#endif
#endif

#define INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_NAME             "unknown"
#define INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_TYPE             "unknown"
#define INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_FIRMWARE_VERSION INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_PROJECT_VERSION

typedef struct {
    const char* project_name;
    const char* project_version;
    const char* name;
    const char* type;
    const char* firmware_version;
} inf_system_info_esp_impl_cfg_t;

#define INF_SYSTEM_INFO_ESP_IMPL_CFG_DEFAULT()                                 \
    {                                                                          \
        .project_name     = INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_PROJECT_NAME,     \
        .project_version  = INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_PROJECT_VERSION,  \
        .name             = INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_NAME,             \
        .type             = INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_TYPE,             \
        .firmware_version = INF_SYSTEM_INFO_ESP_IMPL_DEFAULT_FIRMWARE_VERSION, \
    }

typedef struct {
    inf_system_info_esp_impl_cfg_t cfg;
} inf_system_info_esp_impl_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_SYSTEM_INFO_ESP_IMPL_TYPES_H */
