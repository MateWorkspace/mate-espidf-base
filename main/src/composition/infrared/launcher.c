#include "composition/infrared/launcher.h"

#include <stdbool.h>
#include <string.h>

#include "composition/infrared/application.h"
#include "composition/infrared/driver.h"
#include "composition/infrared/infrastructure.h"
#include "composition/infrared/presentation.h"
#include "composition/infrared/types.h"
#include "domain/models/error.h"
#include "domain/models/preloaded.h"
#include "esp_log.h"

#define TAG "cmp_infrared_launcher"

void cmp_infrared_launcher(void) {
    bool init_driver         = false;
    bool init_infrastructure = false;
    bool init_application    = false;
    bool init_presentation   = false;

    cmp_infrared_launcher_t launcher;
    memset(&launcher, 0, sizeof(launcher));

    dom_models_error_t err = cmp_infrared_driver_init(&launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to initialize driver stage: %s (%d)", dom_models_error_str(err), (int)err);
        goto fail;
    }
    init_driver = true;

    err = cmp_infrared_infrastructure_init(&launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to initialize infrastructure stage: %s (%d)", dom_models_error_str(err), (int)err);
        goto fail;
    }
    init_infrastructure = true;

    err = cmp_infrared_application_init(&launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to initialize application stage: %s (%d)", dom_models_error_str(err), (int)err);
        goto fail;
    }
    init_application = true;

    err = cmp_infrared_presentation_init(&launcher);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to initialize presentation stage: %s (%d)", dom_models_error_str(err), (int)err);
        goto fail;
    }
    init_presentation = true;

    ESP_LOGI(TAG, "Main composition initialized successfully");

    err = launcher.application.ota->validate(launcher.application.ota);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to validate running partition: %s (%d)", dom_models_error_str(err), (int)err);
        (void)launcher.application.ota->rollback(launcher.application.ota);
        goto fail;
    }

    launcher.application.settings->restart(launcher.application.settings, dom_models_preloaded_data.system_restart_after_ms);

fail:
    if (init_presentation) {
        cmp_infrared_presentation_deinit(&launcher);
    }
    if (init_application) {
        cmp_infrared_application_deinit(&launcher);
    }
    if (init_infrastructure) {
        cmp_infrared_infrastructure_deinit(&launcher);
    }
    if (init_driver) {
        cmp_infrared_driver_deinit(&launcher);
    }
}
