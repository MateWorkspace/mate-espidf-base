#include "infrared_tx.h"

#include <stdlib.h>

#include "driver/rmt_common.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/semphr.h"
#include "hal/rmt_types.h"
#include "infrared_internal.h"

#define INFRARED_TX_MEM_BLOCK_SYMBOLS 64
#define INFRARED_TX_QUEUE_DEPTH       4
#define INFRARED_TX_DONE_TIMEOUT_MS   10000

static size_t infrared_tx_duration_parts(uint32_t duration_us) {
    if (duration_us == 0) {
        return 0;
    }

    return (duration_us + INFRARED_TX_MAX_RMT_DURATION_US - 1) / INFRARED_TX_MAX_RMT_DURATION_US;
}

static esp_err_t infrared_tx_count_parts(
    const infrared_duration_t* durations,
    size_t                     duration_count,
    size_t*                    out_part_count
) {
    if (!durations || duration_count == 0 || !out_part_count) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t part_count = 0;
    for (size_t i = 0; i < duration_count; i++) {
        const size_t parts = infrared_tx_duration_parts(durations[i].duration_us);
        if (parts == 0) {
            return ESP_ERR_INVALID_ARG;
        }
        part_count += parts;
    }

    *out_part_count = part_count;
    return ESP_OK;
}

static void infrared_tx_set_symbol_part(
    rmt_symbol_word_t* symbol,
    size_t             part_index,
    uint16_t           duration,
    bool               level
) {
    if ((part_index % 2) == 0) {
        symbol->duration0 = duration;
        symbol->level0    = level;
        return;
    }

    symbol->duration1 = duration;
    symbol->level1    = level;
}

static esp_err_t infrared_tx_build_symbols(
    const infrared_duration_t* durations,
    size_t                     duration_count,
    rmt_symbol_word_t**        out_symbols,
    size_t*                    out_symbol_count
) {
    size_t    part_count = 0;
    esp_err_t err        = infrared_tx_count_parts(durations, duration_count, &part_count);
    if (err != ESP_OK) {
        return err;
    }

    const size_t       symbol_count = (part_count + 1) / 2;
    rmt_symbol_word_t* symbols      = (rmt_symbol_word_t*)calloc(symbol_count, sizeof(rmt_symbol_word_t));
    if (!symbols) {
        return ESP_ERR_NO_MEM;
    }

    size_t part_index = 0;
    for (size_t i = 0; i < duration_count; i++) {
        uint32_t remaining = durations[i].duration_us;
        while (remaining > 0) {
            const uint16_t     part_duration = remaining > INFRARED_TX_MAX_RMT_DURATION_US
                                                   ? INFRARED_TX_MAX_RMT_DURATION_US
                                                   : (uint16_t)remaining;
            rmt_symbol_word_t* symbol        = &symbols[part_index / 2];
            infrared_tx_set_symbol_part(symbol, part_index, part_duration, durations[i].level);
            remaining -= part_duration;
            part_index++;
        }
    }

    if ((part_count % 2) != 0) {
        infrared_tx_set_symbol_part(&symbols[symbol_count - 1], part_count, 1, false);
    }

    *out_symbols      = symbols;
    *out_symbol_count = symbol_count;
    return ESP_OK;
}

esp_err_t infrared_tx_init(infrared_handle_t* self) {
    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    if (self->tx_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    self->tx_mutex = xSemaphoreCreateMutex();
    if (!self->tx_mutex) {
        return ESP_ERR_NO_MEM;
    }

    const rmt_tx_channel_config_t tx_channel_cfg = {
        .gpio_num          = self->cfg.tx_gpio,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = INFRARED_TX_RESOLUTION_HZ,
        .mem_block_symbols = INFRARED_TX_MEM_BLOCK_SYMBOLS,
        .trans_queue_depth = INFRARED_TX_QUEUE_DEPTH,
        .intr_priority     = 0,
        .flags             = {
            .invert_out   = false,
            .with_dma     = false,
            .io_loop_back = false,
            .io_od_mode   = false,
            .allow_pd     = false,
            .init_level   = false,
        },
    };

    esp_err_t err = rmt_new_tx_channel(&tx_channel_cfg, &self->tx_channel);
    if (err != ESP_OK) {
        infrared_tx_deinit(self);
        return err;
    }

    const rmt_carrier_config_t carrier_cfg = {
        .frequency_hz = INFRARED_TX_CARRIER_HZ,
        .duty_cycle   = 0.33,
        .flags        = {
            .polarity_active_low = false,
            .always_on           = false,
        },
    };

    err = rmt_apply_carrier(self->tx_channel, &carrier_cfg);
    if (err != ESP_OK) {
        infrared_tx_deinit(self);
        return err;
    }

    const rmt_copy_encoder_config_t encoder_cfg = {};
    err                                         = rmt_new_copy_encoder(&encoder_cfg, &self->tx_encoder);
    if (err != ESP_OK) {
        infrared_tx_deinit(self);
        return err;
    }

    err = rmt_enable(self->tx_channel);
    if (err != ESP_OK) {
        infrared_tx_deinit(self);
        return err;
    }

    self->tx_enabled     = true;
    self->tx_initialized = true;
    return ESP_OK;
}

void infrared_tx_deinit(infrared_handle_t* self) {
    if (!self) {
        return;
    }

    if (self->tx_channel) {
        (void)rmt_tx_wait_all_done(self->tx_channel, 1000);
    }

    if (self->tx_enabled && self->tx_channel) {
        (void)rmt_disable(self->tx_channel);
        self->tx_enabled = false;
    }

    if (self->tx_encoder) {
        (void)rmt_del_encoder(self->tx_encoder);
        self->tx_encoder = NULL;
    }

    if (self->tx_channel) {
        (void)rmt_del_channel(self->tx_channel);
        self->tx_channel = NULL;
    }

    if (self->tx_mutex) {
        vSemaphoreDelete(self->tx_mutex);
        self->tx_mutex = NULL;
    }

    self->tx_initialized = false;
}

esp_err_t infrared_tx_transmit(
    infrared_handle_t*         self,
    const infrared_duration_t* durations,
    size_t                     duration_count
) {
    if (!self || !durations || duration_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!self->tx_initialized || !self->tx_enabled || !self->tx_channel || !self->tx_encoder) {
        return ESP_ERR_INVALID_STATE;
    }

    rmt_symbol_word_t* symbols      = NULL;
    size_t             symbol_count = 0;
    esp_err_t          err          = infrared_tx_build_symbols(durations, duration_count, &symbols, &symbol_count);
    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(self->tx_mutex, portMAX_DELAY) != pdTRUE) {
        free(symbols);
        return ESP_ERR_TIMEOUT;
    }

    const rmt_transmit_config_t transmit_cfg = {
        .loop_count = 0,
        .flags      = {
            .eot_level         = false,
            .queue_nonblocking = false,
        },
    };

    err = rmt_transmit(
        self->tx_channel,
        self->tx_encoder,
        symbols,
        symbol_count * sizeof(rmt_symbol_word_t),
        &transmit_cfg
    );
    if (err == ESP_OK) {
        err = rmt_tx_wait_all_done(self->tx_channel, INFRARED_TX_DONE_TIMEOUT_MS);
    }

    xSemaphoreGive(self->tx_mutex);
    free(symbols);
    return err;
}
