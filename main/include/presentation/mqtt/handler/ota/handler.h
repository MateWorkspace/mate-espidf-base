#ifndef PRESENTATION_MQTT_HANDLER_OTA_HANDLER_H
#define PRESENTATION_MQTT_HANDLER_OTA_HANDLER_H

#include "presentation/mqtt/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void pres_mqtt_handler_ota(pres_mqtt_context_t* ctx, const char* data, int data_len);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_OTA_HANDLER_H */
