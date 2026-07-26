#ifndef PRESENTATION_MQTT_EVENT_ON_MESSAGE_H
#define PRESENTATION_MQTT_EVENT_ON_MESSAGE_H

#include "mqtt_client.h"
#include "presentation/mqtt/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void pres_mqtt_event_on_message(pres_mqtt_context_t* ctx, esp_mqtt_event_handle_t event);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_EVENT_ON_MESSAGE_H */
