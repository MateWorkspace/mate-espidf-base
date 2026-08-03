#include "composition/counter_test/launcher.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "composition/counter_test/application.h"
#include "composition/counter_test/driver.h"
#include "composition/counter_test/infrastructure.h"
#include "composition/counter_test/presentation.h"
#include "composition/counter_test/types.h"
#include "domain/models/error.h"
#include "domain/models/preloaded.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"

#define TAG "cmp_counter_test_launcher"

#define COUNTER_TELEMETRY_METRIC_NAME    "Counter Test"
#define COUNTER_TELEMETRY_SCHEMA_NAME    "counter"
#define COUNTER_TELEMETRY_SCHEMA_VERSION 1
#define COUNTER_TELEMETRY_INTERVAL_MS    10000

void cmp_counter_test_launcher(void) {
    bool init_driver         = false;
    bool init_infrastructure = false;
    bool init_application    = false;
    bool init_presentation   = false;

    cmp_counter_test_launcher_t launcher;
    memset(&launcher, 0, sizeof(launcher));

    dom_models_error_t err = cmp_counter_test_driver_init(&launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to initialize driver stage: %s (%d)", dom_models_error_str(err), (int)err);
        goto fail;
    }
    init_driver = true;

    err = cmp_counter_test_infrastructure_init(&launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to initialize infrastructure stage: %s (%d)", dom_models_error_str(err), (int)err);
        goto fail;
    }
    init_infrastructure = true;

    err = cmp_counter_test_application_init(&launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to initialize application stage: %s (%d)", dom_models_error_str(err), (int)err);
        goto fail;
    }
    init_application = true;

    err = cmp_counter_test_presentation_init(&launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to initialize presentation stage: %s (%d)", dom_models_error_str(err), (int)err);
        goto fail;
    }
    init_presentation = true;

    ESP_LOGI(TAG, "Counter test composition initialized successfully");

    err = launcher.application.ota->validate(launcher.application.ota);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to validate running partition: %s (%d)", dom_models_error_str(err), (int)err);
        (void)launcher.application.ota->rollback(launcher.application.ota);
        goto fail;
    }

    unsigned long count = 0;
    for (;;) {
        char payload_json[32];
        int  written = snprintf(payload_json, sizeof(payload_json), "{\"count\":%lu}", count);
        if (written > 0 && (size_t)written < sizeof(payload_json)) {
            dom_models_error_t pub_err = launcher.infrastructure.def_pub->telemetry(
                launcher.infrastructure.def_pub,
                dom_models_preloaded_data.device_id_str,
                COUNTER_TELEMETRY_METRIC_NAME,
                COUNTER_TELEMETRY_SCHEMA_NAME,
                COUNTER_TELEMETRY_SCHEMA_VERSION,
                payload_json
            );
            if (pub_err != DOMAIN_MODELS_ERROR_OK) {
                ESP_LOGW(TAG, "Failed to publish counter telemetry: %s (%d)", dom_models_error_str(pub_err), (int)pub_err);
            }
            launcher.infrastructure.logger->info(launcher.infrastructure.logger, TAG, "Published counter telemetry: %s", payload_json);
        }

        count++;
        vTaskDelay(pdMS_TO_TICKS(COUNTER_TELEMETRY_INTERVAL_MS));
    }

    launcher.application.settings->restart(launcher.application.settings, dom_models_preloaded_data.system_restart_after_ms);

fail:
    if (init_presentation) {
        cmp_counter_test_presentation_deinit(&launcher);
    }
    if (init_application) {
        cmp_counter_test_application_deinit(&launcher);
    }
    if (init_infrastructure) {
        cmp_counter_test_infrastructure_deinit(&launcher);
    }
    if (init_driver) {
        cmp_counter_test_driver_deinit(&launcher);
    }
}
