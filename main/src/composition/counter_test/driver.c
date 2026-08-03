#include "composition/counter_test/driver.h"

#include <stdbool.h>
#include <string.h>

#include "composition/counter_test/preloaded.h"
#include "composition/counter_test/utils.h"
#include "domain/models/preloaded.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "host/ble_hs.h"
#include "mqtt_client.h"
#include "nimble/nimble_port.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define CMP_COUNTER_TEST_DRIVER_MQTT_PROTO_TLS "mqtts"

/* Helper Function Prototypes */

static dom_models_error_t init_nvs(cmp_counter_test_launcher_t* launcher);
static dom_models_error_t init_mqtt_client(cmp_counter_test_launcher_t* launcher);
static dom_models_error_t init_ble(cmp_counter_test_launcher_t* launcher);

/* Constructor and Destructor */

dom_models_error_t cmp_counter_test_driver_init(cmp_counter_test_launcher_t* launcher) {
    if (!launcher) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    dom_models_error_t err = init_nvs(launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = cmp_counter_test_preloaded_load_from_nvs(launcher->driver.nvs);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    esp_err_t esp_err = esp_event_loop_create_default();
    if (esp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    esp_err = esp_netif_init();
    if (esp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err                     = esp_wifi_init(&wifi_cfg);
    if (esp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    err = init_ble(launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = init_mqtt_client(launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

void cmp_counter_test_driver_deinit(cmp_counter_test_launcher_t* launcher) {
    if (!launcher) {
        return;
    }

    if (launcher->driver.mqtt_client) {
        esp_mqtt_client_stop(launcher->driver.mqtt_client);
        esp_mqtt_client_destroy(launcher->driver.mqtt_client);
        launcher->driver.mqtt_client = NULL;
    }

    nimble_port_deinit();

    esp_wifi_deinit();
    esp_netif_deinit();
    esp_event_loop_delete_default();

    cmp_counter_test_preloaded_free();

    if (launcher->driver.nvs) {
        nvs_close(launcher->driver.nvs);
        launcher->driver.nvs = 0;
    }
}

/* Helper Function Implementations */

static dom_models_error_t init_nvs(cmp_counter_test_launcher_t* launcher) {
    esp_err_t esp_err = nvs_flash_init();
    if (esp_err == ESP_ERR_NVS_NO_FREE_PAGES || esp_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        esp_err = nvs_flash_erase();
        if (esp_err != ESP_OK) {
            return DOMAIN_MODELS_ERROR_FAILURE;
        }
        esp_err = nvs_flash_init();
    }
    if (esp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    esp_err = nvs_open("mate", NVS_READWRITE, &launcher->driver.nvs);
    if (esp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t init_ble(cmp_counter_test_launcher_t* launcher) {
    (void)launcher;

    esp_err_t esp_err = nimble_port_init();
    if (esp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();

    char device_name[40];
    dom_models_error_t err = cmp_counter_test_utils_build_ble_device_name(device_name, sizeof(device_name));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    int rc = ble_svc_gap_device_name_set(device_name);
    if (rc != 0) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t init_mqtt_client(cmp_counter_test_launcher_t* launcher) {
    char               client_id[64];
    dom_models_error_t err = cmp_counter_test_utils_build_mqtt_client_id(client_id, sizeof(client_id));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    char broker_uri[160];
    err = cmp_counter_test_utils_build_mqtt_broker_uri(broker_uri, sizeof(broker_uri));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    char lwt_topic[64];
    err = cmp_counter_test_utils_build_mqtt_lwt_topic(lwt_topic, sizeof(lwt_topic));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    esp_mqtt_client_config_t mqtt_cfg;
    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));

    mqtt_cfg.broker.address.uri           = broker_uri;
    mqtt_cfg.credentials.client_id        = client_id;
    mqtt_cfg.session.last_will.topic      = lwt_topic;
    mqtt_cfg.session.last_will.msg        = "{\"status\":\"OFFLINE\"}";
    mqtt_cfg.session.last_will.qos        = 1;
    mqtt_cfg.session.last_will.retain     = true;
    mqtt_cfg.network.reconnect_timeout_ms = 5000;
    mqtt_cfg.buffer.size                  = 4096;
    mqtt_cfg.buffer.out_size              = 4096;

    if (cmp_counter_test_utils_cstr_available(dom_models_preloaded_data.mqtt_user)) {
        mqtt_cfg.credentials.username = dom_models_preloaded_data.mqtt_user;
        if (cmp_counter_test_utils_cstr_available(dom_models_preloaded_data.mqtt_pass)) {
            mqtt_cfg.credentials.authentication.password = dom_models_preloaded_data.mqtt_pass;
        }
    }

    if (strcmp(dom_models_preloaded_data.mqtt_proto, CMP_COUNTER_TEST_DRIVER_MQTT_PROTO_TLS) == 0) {
        mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    }

    launcher->driver.mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!launcher->driver.mqtt_client) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    esp_err_t esp_err = esp_mqtt_client_start(launcher->driver.mqtt_client);
    if (esp_err != ESP_OK) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
