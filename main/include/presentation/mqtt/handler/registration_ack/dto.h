#ifndef PRESENTATION_MQTT_HANDLER_REGISTRATION_ACK_DTO_H
#define PRESENTATION_MQTT_HANDLER_REGISTRATION_ACK_DTO_H

#include <stdbool.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool success;
    bool success_set;
} pres_mqtt_handler_registration_ack_dto_request_t;

dom_models_error_t pres_mqtt_handler_registration_ack_dto_decode(
    const char*                                        data,
    int                                                 data_len,
    pres_mqtt_handler_registration_ack_dto_request_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_REGISTRATION_ACK_DTO_H */
