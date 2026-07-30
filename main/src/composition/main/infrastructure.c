#include "composition/main/infrastructure.h"

#include "domain/models/logger.h"
#include "infrastructure/device/wifi/esp_impl.h"
#include "infrastructure/logger/leveled/stdio_impl.h"
#include "infrastructure/messaging/def_pub/mqtt_impl.h"
#include "infrastructure/messaging/def_sub/mqtt_impl.h"
#include "infrastructure/repository/preloaded/nvs_impl.h"
#include "infrastructure/repository/wifi/nvs_impl.h"
#include "infrastructure/system/info/esp_impl.h"
#include "infrastructure/system/restart/esp_impl.h"
#include "infrastructure/system/update/esp_https_impl.h"

#include "esp_netif_sntp.h"

#define NTP_SERVER "pool.ntp.org"

dom_models_error_t cmp_main_infrastructure_init(cmp_main_launcher_t* launcher) {
    if (!launcher) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_logger_leveled_stdio_impl_cfg_t logger_cfg = {
        .level      = DOMAIN_MODELS_LOGGER_LEVEL_INFO,
        /* Exactly one direct subscriber: the log_forwarding usecase, which
           fans out to its own sinks (MQTT, BLE) internally. */
        .cb_max_cnt = 1,
    };
    launcher->infrastructure.logger = inf_logger_leveled_stdio_impl_new(&logger_cfg);
    if (!launcher->infrastructure.logger) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    launcher->infrastructure.system_info = inf_system_info_esp_impl_new(NULL);
    if (!launcher->infrastructure.system_info) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    launcher->infrastructure.system_restart = inf_system_restart_esp_impl_new(NULL);
    if (!launcher->infrastructure.system_restart) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    launcher->infrastructure.system_update = inf_system_update_esp_https_impl_new(NULL);
    if (!launcher->infrastructure.system_update) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    inf_repository_preloaded_nvs_impl_cfg_t preloaded_repository_cfg = {
        .nvs = launcher->driver.nvs,
    };
    launcher->infrastructure.preloaded_repository = inf_repository_preloaded_nvs_impl_new(&preloaded_repository_cfg);
    if (!launcher->infrastructure.preloaded_repository) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    inf_repository_wifi_nvs_impl_cfg_t wifi_repository_cfg = {
        .nvs = launcher->driver.nvs,
    };
    launcher->infrastructure.wifi_repository = inf_repository_wifi_nvs_impl_new(&wifi_repository_cfg);
    if (!launcher->infrastructure.wifi_repository) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    launcher->infrastructure.wifi = inf_device_wifi_esp_impl_new(NULL);
    if (!launcher->infrastructure.wifi) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    dom_models_error_t err = inf_device_wifi_esp_impl_init(launcher->infrastructure.wifi);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    esp_err_t          sntp_err = esp_netif_sntp_init(&sntp_cfg);
    if (sntp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    inf_messaging_def_pub_mqtt_impl_cfg_t def_pub_cfg = {
        .mqtt_client = launcher->driver.mqtt_client,
    };
    launcher->infrastructure.def_pub = inf_messaging_def_pub_mqtt_impl_new(&def_pub_cfg);
    if (!launcher->infrastructure.def_pub) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    inf_messaging_def_sub_mqtt_impl_cfg_t def_sub_cfg = {
        .mqtt_client = launcher->driver.mqtt_client,
    };
    launcher->infrastructure.def_sub = inf_messaging_def_sub_mqtt_impl_new(&def_sub_cfg);
    if (!launcher->infrastructure.def_sub) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

void cmp_main_infrastructure_deinit(cmp_main_launcher_t* launcher) {
    if (!launcher) {
        return;
    }

    if (launcher->infrastructure.def_sub) {
        inf_messaging_def_sub_mqtt_impl_delete(launcher->infrastructure.def_sub);
        launcher->infrastructure.def_sub = NULL;
    }

    if (launcher->infrastructure.def_pub) {
        inf_messaging_def_pub_mqtt_impl_delete(launcher->infrastructure.def_pub);
        launcher->infrastructure.def_pub = NULL;
    }

    if (launcher->infrastructure.wifi) {
        (void)inf_device_wifi_esp_impl_deinit(launcher->infrastructure.wifi);
        inf_device_wifi_esp_impl_delete(launcher->infrastructure.wifi);
        launcher->infrastructure.wifi = NULL;
    }

    if (launcher->infrastructure.wifi_repository) {
        inf_repository_wifi_nvs_impl_delete(launcher->infrastructure.wifi_repository);
        launcher->infrastructure.wifi_repository = NULL;
    }

    if (launcher->infrastructure.preloaded_repository) {
        inf_repository_preloaded_nvs_impl_delete(launcher->infrastructure.preloaded_repository);
        launcher->infrastructure.preloaded_repository = NULL;
    }

    if (launcher->infrastructure.system_update) {
        inf_system_update_esp_https_impl_delete(launcher->infrastructure.system_update);
        launcher->infrastructure.system_update = NULL;
    }

    if (launcher->infrastructure.system_restart) {
        inf_system_restart_esp_impl_delete(launcher->infrastructure.system_restart);
        launcher->infrastructure.system_restart = NULL;
    }

    if (launcher->infrastructure.system_info) {
        inf_system_info_esp_impl_delete(launcher->infrastructure.system_info);
        launcher->infrastructure.system_info = NULL;
    }

    if (launcher->infrastructure.logger) {
        inf_logger_leveled_stdio_impl_delete(launcher->infrastructure.logger);
        launcher->infrastructure.logger = NULL;
    }
}
