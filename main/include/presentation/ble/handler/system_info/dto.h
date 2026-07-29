#ifndef PRESENTATION_BLE_HANDLER_SYSTEM_INFO_DTO_H
#define PRESENTATION_BLE_HANDLER_SYSTEM_INFO_DTO_H

#include <stddef.h>

#include "domain/models/preloaded.h"
#include "domain/models/system.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t pres_ble_handler_system_info_dto_encode_info(
    const dom_models_system_project_info_t* project,
    const dom_models_system_chip_info_t*    chip,
    char*                                   buf,
    size_t                                  buf_cap
);

size_t pres_ble_handler_system_info_dto_encode_config_schema(
    const dom_models_preloaded_schema_entry_t* entries,
    size_t                                     count,
    char*                                      buf,
    size_t                                     buf_cap
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HANDLER_SYSTEM_INFO_DTO_H */
