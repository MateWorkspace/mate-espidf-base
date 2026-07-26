#include "composition/main/presentation.h"

#include "mqtt_client.h"
#include "presentation/mqtt/context.h"
#include "presentation/mqtt/event/event_handler.h"
#include "presentation/task/wifi_sta_reconnect/task.h"

dom_models_error_t cmp_main_presentation_init(cmp_main_launcher_t* launcher) {
    if (!launcher) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    launcher->presentation.mqtt_context = pres_mqtt_context_new(
        launcher->infrastructure.logger,
        launcher->infrastructure.preloaded_repository,
        launcher->application.messaging_callbacks,
        launcher->application.ota
    );
    if (!launcher->presentation.mqtt_context) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    esp_err_t esp_err = esp_mqtt_client_register_event(
        launcher->driver.mqtt_client,
        ESP_EVENT_ANY_ID,
        pres_mqtt_event_handler,
        launcher->presentation.mqtt_context
    );
    if (esp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    pres_task_wifi_sta_reconnect_cfg_t reconnect_task_cfg = {
        .wifi_manager = launcher->application.wifi_manager,
    };
    launcher->presentation.wifi_sta_reconnect_task = pres_task_wifi_sta_reconnect_new(&reconnect_task_cfg);
    if (!launcher->presentation.wifi_sta_reconnect_task) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    dom_models_error_t err = pres_task_wifi_sta_reconnect_start(launcher->presentation.wifi_sta_reconnect_task);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

void cmp_main_presentation_deinit(cmp_main_launcher_t* launcher) {
    if (!launcher) {
        return;
    }

    if (launcher->presentation.wifi_sta_reconnect_task) {
        pres_task_wifi_sta_reconnect_delete(launcher->presentation.wifi_sta_reconnect_task);
        launcher->presentation.wifi_sta_reconnect_task = NULL;
    }

    if (launcher->driver.mqtt_client) {
        esp_mqtt_client_unregister_event(
            launcher->driver.mqtt_client,
            ESP_EVENT_ANY_ID,
            pres_mqtt_event_handler
        );
    }

    if (launcher->presentation.mqtt_context) {
        pres_mqtt_context_delete(launcher->presentation.mqtt_context);
        launcher->presentation.mqtt_context = NULL;
    }
}
