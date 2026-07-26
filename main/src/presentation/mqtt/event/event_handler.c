#include "presentation/mqtt/event/event_handler.h"

#include "mqtt_client.h"
#include "presentation/mqtt/context.h"
#include "presentation/mqtt/event/on_connect.h"
#include "presentation/mqtt/event/on_disconnect.h"
#include "presentation/mqtt/event/on_error.h"
#include "presentation/mqtt/event/on_message.h"

void pres_mqtt_event_handler(
    void*            handler_args,
    esp_event_base_t base,
    int32_t          event_id,
    void*            event_data
) {
    (void)base;

    pres_mqtt_context_t* ctx = (pres_mqtt_context_t*)handler_args;
    if (!ctx) {
        return;
    }

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            pres_mqtt_event_on_connect(ctx, event);
            break;
        case MQTT_EVENT_DISCONNECTED:
            pres_mqtt_event_on_disconnect(ctx, event);
            break;
        case MQTT_EVENT_DATA:
            pres_mqtt_event_on_message(ctx, event);
            break;
        case MQTT_EVENT_ERROR:
            pres_mqtt_event_on_error(ctx, event);
            break;
        default:
            break;
    }
}
