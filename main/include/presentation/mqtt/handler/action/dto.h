#ifndef PRESENTATION_MQTT_HANDLER_ACTION_DTO_H
#define PRESENTATION_MQTT_HANDLER_ACTION_DTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_MQTT_HANDLER_ACTION_DTO_EXECUTION_ID_MAX_LEN 64
#define PRES_MQTT_HANDLER_ACTION_DTO_ACTION_MAX_LEN        32

typedef struct {
    char     execution_id[PRES_MQTT_HANDLER_ACTION_DTO_EXECUTION_ID_MAX_LEN];
    bool     execution_id_set;
    char     action[PRES_MQTT_HANDLER_ACTION_DTO_ACTION_MAX_LEN];
    bool     action_set;
    uint32_t delay_ms;
} pres_mqtt_handler_action_dto_request_t;

/* Parses the /sub/<device_id>/action payload. Returns
   DOMAIN_MODELS_ERROR_BAD_ARGUMENT if data is empty/not valid JSON -
   individual missing-field cases are reported via the _set flags instead
   of a decode error, matching the existing negative-case behavior
   (ACT-05/ACT-06 in docs/agent_test/v1.0.0-dev.1/scenario/05-action-dispatch.md)
   where a missing execution_id is silently dropped by the caller and a
   missing action field gets an explicit FAILED ack. */
dom_models_error_t pres_mqtt_handler_action_dto_decode(
    const char*                             data,
    int                                     data_len,
    pres_mqtt_handler_action_dto_request_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_ACTION_DTO_H */
