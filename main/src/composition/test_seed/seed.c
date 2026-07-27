#include "composition/test_seed/seed.h"

#include <stdbool.h>
#include <string.h>

#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/repository/wifi.h"
#include "domain/models/error.h"
#include "domain/models/wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "infrastructure/repository/preloaded/nvs_impl.h"
#include "infrastructure/repository/wifi/nvs_impl.h"
#include "nvs.h"
#include "nvs_flash.h"

#define TAG "test_seed"

/* Test-only credentials. WiFi AP is a real local access point used for
   hardware testing; MQTT values are copied from mate-things/.env's real
   HiveMQ Cloud broker so the seeded device can register against the same
   backend instance used for docs/agent_test/. Never used for anything
   deployed - this whole file only builds when
   CONFIG_MATE_TEST_SEED_NVS_ON_BOOT is set. */
#define TEST_WIFI_SSID     "Kartono internet"
#define TEST_WIFI_PASSWORD "KamiGendut123"

#define TEST_MQTT_PROTO "mqtts"
#define TEST_MQTT_HOST  "e7c3d891e00d426bbab8a7b33fcd079e.s1.eu.hivemq.cloud"
#define TEST_MQTT_PORT  "8883"
#define TEST_MQTT_USER  "matemqtt"
#define TEST_MQTT_PASS  "mate12345678"

static esp_err_t init_nvs(nvs_handle_t* out_handle) {
    esp_err_t esp_err = nvs_flash_init();
    if (esp_err == ESP_ERR_NVS_NO_FREE_PAGES || esp_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        esp_err = nvs_flash_erase();
        if (esp_err != ESP_OK) {
            return esp_err;
        }
        esp_err = nvs_flash_init();
    }
    if (esp_err != ESP_OK) {
        return esp_err;
    }

    return nvs_open("mate", NVS_READWRITE, out_handle);
}

static bool check(dom_models_error_t err, const char* what) {
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to set %s: %s (%d)", what, dom_models_error_str(err), (int)err);
        return false;
    }
    ESP_LOGI(TAG, "Set %s", what);
    return true;
}

void cmp_test_seed_run(void) {
    ESP_LOGW(TAG, "TEST SEED BUILD - writing fixed test credentials into NVS \"mate\" namespace");

    nvs_handle_t nvs_handle;
    esp_err_t    esp_err = init_nvs(&nvs_handle);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init/open NVS: %d", esp_err);
        return;
    }

    inf_repository_preloaded_nvs_impl_cfg_t preloaded_cfg = {.nvs = nvs_handle};
    dom_contracts_repository_preloaded_t*    preloaded    = inf_repository_preloaded_nvs_impl_new(&preloaded_cfg);
    inf_repository_wifi_nvs_impl_cfg_t       wifi_cfg      = {.nvs = nvs_handle};
    dom_contracts_repository_wifi_t*         wifi          = inf_repository_wifi_nvs_impl_new(&wifi_cfg);
    if (!preloaded || !wifi) {
        ESP_LOGE(TAG, "Failed to construct NVS repositories");
        inf_repository_preloaded_nvs_impl_delete(preloaded);
        inf_repository_wifi_nvs_impl_delete(wifi);
        nvs_close(nvs_handle);
        return;
    }

    bool ok = true;
    ok &= check(preloaded->set_mqtt_proto(preloaded, TEST_MQTT_PROTO), "mqtt_proto");
    ok &= check(preloaded->set_mqtt_host(preloaded, TEST_MQTT_HOST), "mqtt_host");
    ok &= check(preloaded->set_mqtt_port(preloaded, TEST_MQTT_PORT), "mqtt_port");
    ok &= check(preloaded->set_mqtt_user(preloaded, TEST_MQTT_USER), "mqtt_user");
    ok &= check(preloaded->set_mqtt_pass(preloaded, TEST_MQTT_PASS), "mqtt_pass");
    ok &= check(preloaded->set_wifi_sta_try_connect_on_init(preloaded, true), "wifi_sta_try_connect_on_init");

    dom_models_wifi_sta_connect_config_t credential;
    memset(&credential, 0, sizeof(credential));
    strncpy(credential.ssid, TEST_WIFI_SSID, sizeof(credential.ssid) - 1);
    strncpy(credential.password, TEST_WIFI_PASSWORD, sizeof(credential.password) - 1);
    ok &= check(wifi->set_sta_credential(wifi, &credential), "wifi_sta_credential");

    inf_repository_preloaded_nvs_impl_delete(preloaded);
    inf_repository_wifi_nvs_impl_delete(wifi);
    nvs_close(nvs_handle);

    if (ok) {
        ESP_LOGW(TAG, "Seed complete. Reflash the normal firmware now (idf.py flash does not touch the nvs partition).");
    } else {
        ESP_LOGE(TAG, "Seed incomplete - see errors above.");
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
