#ifndef PRESENTATION_MQTT_HANDLER_CONFIG_DTO_H
#define PRESENTATION_MQTT_HANDLER_CONFIG_DTO_H

#include <stdbool.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_MQTT_HANDLER_CONFIG_DTO_KEY_MAX_LEN   32
#define PRES_MQTT_HANDLER_CONFIG_DTO_VALUE_MAX_LEN 128

typedef struct {
    char key[PRES_MQTT_HANDLER_CONFIG_DTO_KEY_MAX_LEN];
    bool key_set;
    char value[PRES_MQTT_HANDLER_CONFIG_DTO_VALUE_MAX_LEN];
    bool value_set;
} pres_mqtt_handler_config_dto_request_t;

/* Parses the /sub/<device_id>/config payload's key/value as raw strings -
   type-specific interpretation (uint32/bool parsing) happens in the
   handler, using the preloaded config schema to look up each key's type,
   not here (keeps this DTO layer a pure JSON-shape concern, matching
   every other handler's dto.c in this codebase). */
dom_models_error_t pres_mqtt_handler_config_dto_decode(
    const char*                              data,
    int                                       data_len,
    pres_mqtt_handler_config_dto_request_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_CONFIG_DTO_H */
