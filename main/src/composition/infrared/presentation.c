#include "composition/infrared/presentation.h"

#include <string.h>

#include "composition/infrared/utils.h"
#include "presentation/ble/gatt/registry.h"
#include "presentation/ble/handler/log/handler.h"
#include "presentation/ble/handler/settings/handler.h"
#include "presentation/ble/handler/system_info/handler.h"
#include "presentation/ble/handler/wifi_manager/handler.h"
#include "presentation/ble/host.h"
#include "presentation/mqtt/context.h"
#include "presentation/task/wifi_sta_reconnect/task.h"

dom_models_error_t cmp_infrared_presentation_init(cmp_infrared_launcher_t* launcher) {
    if (!launcher) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    launcher->presentation.mqtt_context = pres_mqtt_context_new(
        launcher->infrastructure.logger,
        launcher->infrastructure.preloaded_repository,
        launcher->application.messaging_callbacks,
        launcher->application.settings,
        launcher->application.ota,
        launcher->infrastructure.def_pub,
        launcher->application.infrared
    );
    if (!launcher->presentation.mqtt_context) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    dom_models_error_t err = pres_mqtt_context_init(launcher->presentation.mqtt_context, launcher->driver.mqtt_client);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    /* ir/tx subscription is issued from pres_mqtt_event_on_connect (fires on
       every MQTT_EVENT_CONNECTED, including reconnects) instead of here -
       subscribing synchronously at boot races the async MQTT connect. */

    pres_task_wifi_sta_reconnect_cfg_t reconnect_task_cfg = {
        .wifi_manager = launcher->application.wifi_manager,
    };
    launcher->presentation.wifi_sta_reconnect_task = pres_task_wifi_sta_reconnect_new(&reconnect_task_cfg);
    if (!launcher->presentation.wifi_sta_reconnect_task) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    err = pres_task_wifi_sta_reconnect_start(launcher->presentation.wifi_sta_reconnect_task);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    launcher->presentation.ble_gatt_registry = pres_ble_gatt_registry_new();
    if (!launcher->presentation.ble_gatt_registry) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    pres_ble_handler_settings_cfg_t ble_settings_cfg = {
        .logger        = launcher->infrastructure.logger,
        .settings      = launcher->application.settings,
        .gatt_registry = launcher->presentation.ble_gatt_registry,
    };
    launcher->presentation.ble_settings = pres_ble_handler_settings_new(&ble_settings_cfg);
    if (!launcher->presentation.ble_settings) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }
    err = pres_ble_handler_settings_init(launcher->presentation.ble_settings);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    pres_ble_handler_wifi_manager_cfg_t ble_wifi_manager_cfg = {
        .logger        = launcher->infrastructure.logger,
        .wifi_manager  = launcher->application.wifi_manager,
        .gatt_registry = launcher->presentation.ble_gatt_registry,
    };
    launcher->presentation.ble_wifi_manager = pres_ble_handler_wifi_manager_new(&ble_wifi_manager_cfg);
    if (!launcher->presentation.ble_wifi_manager) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }
    err = pres_ble_handler_wifi_manager_init(launcher->presentation.ble_wifi_manager);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    pres_ble_handler_system_info_cfg_t ble_system_info_cfg = {
        .logger        = launcher->infrastructure.logger,
        .system_info   = launcher->application.system_info,
        .gatt_registry = launcher->presentation.ble_gatt_registry,
    };
    launcher->presentation.ble_system_info = pres_ble_handler_system_info_new(&ble_system_info_cfg);
    if (!launcher->presentation.ble_system_info) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }
    err = pres_ble_handler_system_info_init(launcher->presentation.ble_system_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    char ble_device_name[40];
    err = cmp_infrared_utils_build_ble_device_name(ble_device_name, sizeof(ble_device_name));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    pres_ble_host_cfg_t ble_host_cfg = {
        .logger        = launcher->infrastructure.logger,
        .gatt_registry = launcher->presentation.ble_gatt_registry,
    };
    memcpy(ble_host_cfg.device_name, ble_device_name, sizeof(ble_host_cfg.device_name));
    launcher->presentation.ble_host = pres_ble_host_new(&ble_host_cfg);
    if (!launcher->presentation.ble_host) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    pres_ble_handler_log_cfg_t ble_log_cfg = {
        .logger         = launcher->infrastructure.logger,
        .log_forwarding = launcher->application.log_forwarding,
        .gatt_registry  = launcher->presentation.ble_gatt_registry,
        .host           = launcher->presentation.ble_host,
    };
    launcher->presentation.ble_log = pres_ble_handler_log_new(&ble_log_cfg);
    if (!launcher->presentation.ble_log) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }
    err = pres_ble_handler_log_init(launcher->presentation.ble_log);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    /* Started last: registers every service accumulated above into NimBLE
       (all ble_gatts_add_svcs() calls must complete before the host syncs)
       and only then starts advertising. */
    err = pres_ble_host_start(launcher->presentation.ble_host);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

void cmp_infrared_presentation_deinit(cmp_infrared_launcher_t* launcher) {
    if (!launcher) {
        return;
    }

    if (launcher->presentation.ble_host) {
        pres_ble_host_delete(launcher->presentation.ble_host);
        launcher->presentation.ble_host = NULL;
    }

    if (launcher->presentation.ble_log) {
        pres_ble_handler_log_delete(launcher->presentation.ble_log);
        launcher->presentation.ble_log = NULL;
    }

    if (launcher->presentation.ble_wifi_manager) {
        pres_ble_handler_wifi_manager_delete(launcher->presentation.ble_wifi_manager);
        launcher->presentation.ble_wifi_manager = NULL;
    }

    if (launcher->presentation.ble_settings) {
        pres_ble_handler_settings_delete(launcher->presentation.ble_settings);
        launcher->presentation.ble_settings = NULL;
    }

    if (launcher->presentation.ble_system_info) {
        pres_ble_handler_system_info_delete(launcher->presentation.ble_system_info);
        launcher->presentation.ble_system_info = NULL;
    }

    if (launcher->presentation.ble_gatt_registry) {
        pres_ble_gatt_registry_delete(launcher->presentation.ble_gatt_registry);
        launcher->presentation.ble_gatt_registry = NULL;
    }

    if (launcher->presentation.wifi_sta_reconnect_task) {
        pres_task_wifi_sta_reconnect_delete(launcher->presentation.wifi_sta_reconnect_task);
        launcher->presentation.wifi_sta_reconnect_task = NULL;
    }

    if (launcher->presentation.mqtt_context) {
        pres_mqtt_context_deinit(launcher->presentation.mqtt_context);
        pres_mqtt_context_delete(launcher->presentation.mqtt_context);
        launcher->presentation.mqtt_context = NULL;
    }
}
