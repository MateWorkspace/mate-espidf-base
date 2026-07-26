#include "presentation/task/wifi_sta_reconnect/task.h"

#include <stdlib.h>

#include "domain/models/error.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "presentation/task/wifi_sta_reconnect/types.h"
#include "presentation/task/wifi_sta_reconnect/utils.h"

/* Task Function Prototypes */

static void task_impl(void* arg);

/* Constructor and Destructor */

pres_task_wifi_sta_reconnect_t* pres_task_wifi_sta_reconnect_new(
    const pres_task_wifi_sta_reconnect_cfg_t* cfg
) {
    dom_models_error_t err = pres_task_wifi_sta_reconnect_validate_cfg(cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    pres_task_wifi_sta_reconnect_t* self = (pres_task_wifi_sta_reconnect_t*)calloc(1, sizeof(pres_task_wifi_sta_reconnect_t));
    if (!self) {
        return NULL;
    }

    pres_task_wifi_sta_reconnect_normalize_cfg(&self->cfg, cfg);

    return self;
}

void pres_task_wifi_sta_reconnect_delete(
    pres_task_wifi_sta_reconnect_t* self
) {
    if (!self) {
        return;
    }

    (void)pres_task_wifi_sta_reconnect_stop(self);
    free(self);
}

/* Public Function Implementations */

dom_models_error_t pres_task_wifi_sta_reconnect_start(
    pres_task_wifi_sta_reconnect_t* self
) {
    if (!self) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->started) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    self->stop_requested = false;

    BaseType_t result = xTaskCreate(
        task_impl,
        self->cfg.task_name,
        self->cfg.stack_size,
        self,
        self->cfg.priority,
        &self->task_handle
    );
    if (result != pdPASS) {
        self->task_handle    = NULL;
        self->stop_requested = false;
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    self->started = true;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t pres_task_wifi_sta_reconnect_stop(
    pres_task_wifi_sta_reconnect_t* self
) {
    if (!self) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (!self->started) {
        self->task_handle    = NULL;
        self->stop_requested = false;
        return DOMAIN_MODELS_ERROR_OK;
    }

    self->stop_requested = true;

    if (self->task_handle) {
        TaskHandle_t task_handle = self->task_handle;
        self->task_handle        = NULL;
        self->started            = false;
        vTaskDelete(task_handle);
    } else {
        self->started = false;
    }

    self->stop_requested = false;

    return DOMAIN_MODELS_ERROR_OK;
}

/* Task Function Implementations */

static void task_impl(void* arg) {
    pres_task_wifi_sta_reconnect_t* self = (pres_task_wifi_sta_reconnect_t*)arg;
    if (!self) {
        vTaskDelete(NULL);
        return;
    }

    while (!self->stop_requested) {
        bool               needed = false;
        dom_models_error_t err    = self->cfg.wifi_manager->need_reconnect(self->cfg.wifi_manager, &needed);
        if (err == DOMAIN_MODELS_ERROR_OK && needed) {
            bool attempted = false;
            (void)self->cfg.wifi_manager->try_reconnect(self->cfg.wifi_manager, &attempted);
        }

        vTaskDelay(pdMS_TO_TICKS(self->cfg.interval_ms));
    }

    self->task_handle = NULL;
    self->started     = false;

    vTaskDelete(NULL);
}
