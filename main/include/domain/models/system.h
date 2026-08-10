#ifndef DOMAIN_MODELS_SYSTEM_H
#define DOMAIN_MODELS_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char project_name[64];
    char project_version[48];
    char name[64];
    char type[32];
    char firmware_version[48];
    char node_class_name[64];
} dom_models_system_project_info_t;

typedef struct {
    char hardware_mac[18];
    char model[32];
    int  revision;
    int  cores;
} dom_models_system_chip_info_t;

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_MODELS_SYSTEM_H */
