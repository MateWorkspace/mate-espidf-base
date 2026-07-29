#include "presentation/ble/handler/system_info/dto.h"

#include <string.h>

#include "cJSON.h"

static const char* value_type_str(dom_models_preloaded_value_type_t type);

size_t pres_ble_handler_system_info_dto_encode_info(
    const dom_models_system_project_info_t* project,
    const dom_models_system_chip_info_t*    chip,
    char*                                    buf,
    size_t                                   buf_cap
) {
    if (!project || !chip || !buf || buf_cap == 0) {
        return 0;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return 0;
    }

    cJSON* project_obj = cJSON_AddObjectToObject(root, "project");
    if (project_obj) {
        cJSON_AddStringToObject(project_obj, "project_name", project->project_name);
        cJSON_AddStringToObject(project_obj, "project_version", project->project_version);
        cJSON_AddStringToObject(project_obj, "name", project->name);
        cJSON_AddStringToObject(project_obj, "type", project->type);
        cJSON_AddStringToObject(project_obj, "firmware_version", project->firmware_version);
    }

    cJSON* chip_obj = cJSON_AddObjectToObject(root, "chip");
    if (chip_obj) {
        cJSON_AddStringToObject(chip_obj, "hardware_mac", chip->hardware_mac);
        cJSON_AddStringToObject(chip_obj, "model", chip->model);
        cJSON_AddNumberToObject(chip_obj, "revision", chip->revision);
        cJSON_AddNumberToObject(chip_obj, "cores", chip->cores);
    }

    bool ok = cJSON_PrintPreallocated(root, buf, (int)buf_cap, false);
    cJSON_Delete(root);

    return ok ? strnlen(buf, buf_cap) : 0;
}

size_t pres_ble_handler_system_info_dto_encode_config_schema(
    const dom_models_preloaded_schema_entry_t* entries,
    size_t                                      count,
    char*                                       buf,
    size_t                                       buf_cap
) {
    if (!entries || !buf || buf_cap == 0) {
        return 0;
    }

    cJSON* root = cJSON_CreateArray();
    if (!root) {
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        cJSON* entry = cJSON_CreateObject();
        if (!entry) {
            cJSON_Delete(root);
            return 0;
        }

        cJSON_AddStringToObject(entry, "key", entries[i].key);
        cJSON_AddStringToObject(entry, "type", value_type_str(entries[i].type));
        cJSON_AddItemToArray(root, entry);
    }

    bool ok = cJSON_PrintPreallocated(root, buf, (int)buf_cap, false);
    cJSON_Delete(root);

    return ok ? strnlen(buf, buf_cap) : 0;
}

static const char* value_type_str(dom_models_preloaded_value_type_t type) {
    switch (type) {
        case DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING:
            return "string";
        case DOMAIN_MODELS_PRELOADED_VALUE_TYPE_UINT32:
            return "uint32";
        case DOMAIN_MODELS_PRELOADED_VALUE_TYPE_BOOL:
            return "bool";
    }
    return "unknown";
}
