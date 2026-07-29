#ifndef PRESENTATION_MQTT_HANDLER_OTA_DTO_H
#define PRESENTATION_MQTT_HANDLER_OTA_DTO_H

#include "domain/models/error.h"
#include "domain/models/update.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parses the /sub/<device_id>/ota payload into a fully-populated
   dom_models_update_info_t. Returns DOMAIN_MODELS_ERROR_BAD_ARGUMENT if
   data is empty, not valid JSON, or missing/mistyped any of
   firmware_url/firmware_size/firmware_checksum - unlike action's DTO,
   there's no valid partial-decode case for OTA (see ota/impl.c's update_impl,
   which requires all three fields to attempt a download). */
dom_models_error_t pres_mqtt_handler_ota_dto_decode(
    const char*                data,
    int                        data_len,
    dom_models_update_info_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_OTA_DTO_H */
