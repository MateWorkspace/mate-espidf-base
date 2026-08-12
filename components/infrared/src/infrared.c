#include "infrared.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "infrared_internal.h"
#include "infrared_rx.h"
#include "infrared_tx.h"

static bool infrared_is_valid_config(const infrared_cfg_t* cfg) {
    if (!cfg) {
        return false;
    }

    if (!cfg->enable_rx && !cfg->enable_tx) {
        return false;
    }

    if (cfg->enable_rx) {
        if (!GPIO_IS_VALID_GPIO(cfg->rx_gpio) || !cfg->on_receive) {
            return false;
        }
    }

    if (cfg->enable_tx) {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(cfg->tx_gpio)) {
            return false;
        }
    }

    return true;
}

infrared_handle_t* infrared_new(const infrared_cfg_t* cfg) {
    if (!infrared_is_valid_config(cfg)) {
        return NULL;
    }

    infrared_handle_t* self = (infrared_handle_t*)calloc(1, sizeof(infrared_handle_t));
    if (!self) {
        return NULL;
    }

    self->cfg = *cfg;
    self->rx_mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;

    return self;
}

esp_err_t infrared_init(infrared_handle_t* self) {
    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    if (self->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_OK;

    if (self->cfg.enable_rx) {
        err = infrared_rx_init(self);
        if (err != ESP_OK) {
            goto fail;
        }
    }

    if (self->cfg.enable_tx) {
        err = infrared_tx_init(self);
        if (err != ESP_OK) {
            goto fail;
        }
    }

    self->initialized = true;
    return ESP_OK;

fail:
    infrared_deinit(self);
    return err;
}

void infrared_deinit(infrared_handle_t* self) {
    if (!self) {
        return;
    }

    infrared_tx_deinit(self);
    infrared_rx_deinit(self);
    self->initialized = false;
}

void infrared_delete(infrared_handle_t* self) {
    free(self);
}

esp_err_t infrared_transmit(
    infrared_handle_t*         self,
    const infrared_duration_t* durations,
    size_t                     duration_count
) {
    return infrared_tx_transmit(self, durations, duration_count);
}
