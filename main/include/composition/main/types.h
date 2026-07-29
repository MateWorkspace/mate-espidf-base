#ifndef COMPOSITION_MAIN_TYPES_H
#define COMPOSITION_MAIN_TYPES_H

#include "domain/contracts/device/wifi.h"
#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/messaging/def_pub.h"
#include "domain/contracts/messaging/def_sub.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/repository/wifi.h"
#include "domain/contracts/system/info.h"
#include "domain/contracts/system/restart.h"
#include "domain/contracts/system/update.h"
#include "domain/usecases/internal/log_forwarding.h"
#include "domain/usecases/internal/messaging_callbacks.h"
#include "domain/usecases/internal/ota.h"
#include "domain/usecases/internal/settings.h"
#include "domain/usecases/internal/wifi_manager.h"
#include "mqtt_client.h"
#include "nvs.h"
#include "presentation/ble/gatt/registry.h"
#include "presentation/ble/handler/log/handler.h"
#include "presentation/ble/handler/settings/handler.h"
#include "presentation/ble/handler/wifi_manager/handler.h"
#include "presentation/ble/host.h"
#include "presentation/mqtt/context.h"
#include "presentation/task/wifi_sta_reconnect/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t             nvs;
    esp_mqtt_client_handle_t mqtt_client;
} cmp_main_launcher_driver_t;

typedef struct {
    dom_contracts_logger_leveled_t*       logger;
    dom_contracts_system_info_t*          system_info;
    dom_contracts_system_restart_t*       system_restart;
    dom_contracts_system_update_t*        system_update;
    dom_contracts_repository_preloaded_t* preloaded_repository;
    dom_contracts_repository_wifi_t*      wifi_repository;
    dom_contracts_device_wifi_t*          wifi;
    dom_contracts_messaging_def_pub_t*    def_pub;
    dom_contracts_messaging_def_sub_t*    def_sub;
} cmp_main_launcher_infrastructure_t;

typedef struct {
    dom_usecases_internal_ota_t*                 ota;
    dom_usecases_internal_settings_t*            settings;
    dom_usecases_internal_wifi_manager_t*        wifi_manager;
    dom_usecases_internal_log_forwarding_t*      log_forwarding;
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks;
} cmp_main_launcher_application_t;

typedef struct {
    pres_mqtt_context_t*             mqtt_context;
    pres_task_wifi_sta_reconnect_t*  wifi_sta_reconnect_task;
    pres_ble_gatt_registry_t*        ble_gatt_registry;
    pres_ble_handler_settings_t*     ble_settings;
    pres_ble_handler_wifi_manager_t* ble_wifi_manager;
    pres_ble_handler_log_t*          ble_log;
    pres_ble_host_t*                 ble_host;
} cmp_main_launcher_presentation_t;

typedef struct {
    cmp_main_launcher_driver_t         driver;
    cmp_main_launcher_infrastructure_t infrastructure;
    cmp_main_launcher_application_t    application;
    cmp_main_launcher_presentation_t   presentation;
} cmp_main_launcher_t;

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITION_MAIN_TYPES_H */
