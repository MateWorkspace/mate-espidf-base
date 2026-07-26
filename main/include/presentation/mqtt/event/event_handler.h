#ifndef PRESENTATION_MQTT_EVENT_EVENT_HANDLER_H
#define PRESENTATION_MQTT_EVENT_EVENT_HANDLER_H

#include <stdint.h>

#include "esp_event_base.h"

#ifdef __cplusplus
extern "C" {
#endif

void pres_mqtt_event_handler(
    void*            handler_args,
    esp_event_base_t base,
    int32_t          event_id,
    void*            event_data
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_EVENT_EVENT_HANDLER_H */
