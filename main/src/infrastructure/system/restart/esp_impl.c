#include "infrastructure/system/restart/esp_impl.h"

#include <stdlib.h>
#include <string.h>

#include "domain/contracts/system/restart.h"
#include "domain/models/error.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "infrastructure/system/restart/esp_impl_utils.h"

/* Contract Function Prototypes */

static dom_models_error_t restart_impl(
    dom_contracts_system_restart_t* self,
    uint32_t                        delay_ms
);

/* Constructor and Destructor */

dom_contracts_system_restart_t* inf_system_restart_esp_impl_new(const inf_system_restart_esp_impl_cfg_t* cfg) {
    inf_system_restart_esp_impl_ctx_t* ctx = (inf_system_restart_esp_impl_ctx_t*)calloc(1, sizeof(inf_system_restart_esp_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    inf_system_restart_esp_impl_cfg_t default_cfg = INF_SYSTEM_RESTART_ESP_IMPL_CFG_DEFAULT();
    memcpy(&ctx->cfg, cfg ? cfg : &default_cfg, sizeof(inf_system_restart_esp_impl_cfg_t));
    if (inf_system_restart_esp_impl_validate_cfg(&ctx->cfg) != DOMAIN_MODELS_ERROR_OK) {
        free(ctx);
        return NULL;
    }

    dom_contracts_system_restart_t* self = dom_contracts_system_restart_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->restart = restart_impl;

    return self;
}

void inf_system_restart_esp_impl_delete(dom_contracts_system_restart_t* self) {
    if (!self) {
        return;
    }

    free(self->ctx);
    dom_contracts_system_restart_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t restart_impl(
    dom_contracts_system_restart_t* self,
    uint32_t                        delay_ms
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    if (delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    esp_restart();

    return DOMAIN_MODELS_ERROR_OK;
}
