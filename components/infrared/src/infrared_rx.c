#include "infrared_rx.h"

#include <limits.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "infrared_internal.h"

#define INFRARED_RX_TASK_NAME         "infrared_rx"
#define INFRARED_RX_TASK_STOP_WAIT_MS 1000

static TickType_t infrared_rx_idle_ticks(void) {
    const uint32_t timeout_ms = (INFRARED_RX_IDLE_TIMEOUT_US + 999) / 1000;
    TickType_t     ticks      = pdMS_TO_TICKS(timeout_ms);
    return ticks == 0 ? 1 : ticks;
}

static uint32_t infrared_rx_task_stack_size(const infrared_handle_t* self) {
    return self->cfg.rx_task_stack_size == 0 ? INFRARED_RX_TASK_STACK_SIZE_DEFAULT : self->cfg.rx_task_stack_size;
}

static UBaseType_t infrared_rx_task_priority(const infrared_handle_t* self) {
    return self->cfg.rx_task_priority == 0 ? INFRARED_RX_TASK_PRIORITY_DEFAULT : self->cfg.rx_task_priority;
}

static TickType_t infrared_rx_task_poll_ticks(const infrared_handle_t* self) {
    const uint32_t poll_ms = self->cfg.rx_task_poll_ms == 0 ? INFRARED_RX_TASK_POLL_MS_DEFAULT : self->cfg.rx_task_poll_ms;
    TickType_t     ticks   = pdMS_TO_TICKS(poll_ms);
    return ticks == 0 ? 1 : ticks;
}

static UBaseType_t infrared_rx_queue_depth(const infrared_handle_t* self) {
    return self->cfg.rx_queue_depth == 0 ? INFRARED_RX_QUEUE_DEPTH_DEFAULT : (UBaseType_t)self->cfg.rx_queue_depth;
}

static uint32_t infrared_clamp_duration_us(int64_t duration_us) {
    if (duration_us <= 0) {
        return 0;
    }

    if (duration_us > UINT32_MAX) {
        return UINT32_MAX;
    }

    return (uint32_t)duration_us;
}

static void IRAM_ATTR infrared_rx_isr(void* arg) {
    infrared_handle_t* self = (infrared_handle_t*)arg;
    if (!self) {
        return;
    }

    const int64_t now_us = esp_timer_get_time();
    const bool    level  = gpio_get_level(self->cfg.rx_gpio) != 0;

    portENTER_CRITICAL_ISR(&self->rx_mux);

    if (!self->rx_active) {
        self->rx_active       = true;
        self->rx_count        = 0;
        self->rx_overflowed   = false;
        self->rx_last_edge_us = now_us;
        self->rx_last_level   = level;
    } else {
        const uint32_t duration_us = infrared_clamp_duration_us(now_us - self->rx_last_edge_us);
        if (duration_us > 0) {
            if (self->rx_count < INFRARED_RX_MAX_DURATIONS) {
                self->rx_buffer[self->rx_count].level       = self->rx_last_level;
                self->rx_buffer[self->rx_count].duration_us = duration_us;
                self->rx_count++;
            } else {
                self->rx_overflowed = true;
            }
        }

        self->rx_last_edge_us = now_us;
        self->rx_last_level   = level;
    }

    portEXIT_CRITICAL_ISR(&self->rx_mux);

    BaseType_t higher_priority_task_woken = pdFALSE;
    if (self->rx_idle_timer) {
        (void)xTimerResetFromISR(self->rx_idle_timer, &higher_priority_task_woken);
    }
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void infrared_rx_reset_frame_locked(infrared_handle_t* self) {
    self->rx_active       = false;
    self->rx_count        = 0;
    self->rx_overflowed   = false;
    self->rx_last_edge_us = 0;
    self->rx_last_level   = false;
}

static void infrared_rx_idle_timer_cb(TimerHandle_t timer) {
    infrared_handle_t* self = (infrared_handle_t*)pvTimerGetTimerID(timer);
    if (!self || !self->rx_queue) {
        return;
    }

    bool should_queue = false;

    portENTER_CRITICAL(&self->rx_mux);

    if (self->rx_active) {
        if (self->rx_overflowed) {
            self->rx_overflowed_frames++;
        } else if (self->rx_count > 0) {
            self->rx_queue_item.duration_count = self->rx_count;
            self->rx_queue_item.overflowed     = false;
            memcpy(
                self->rx_queue_item.durations,
                self->rx_buffer,
                self->rx_count * sizeof(infrared_duration_t)
            );
            should_queue = true;
        }

        infrared_rx_reset_frame_locked(self);
    }

    portEXIT_CRITICAL(&self->rx_mux);

    if (should_queue) {
        if (xQueueSend(self->rx_queue, &self->rx_queue_item, 0) != pdTRUE) {
            self->rx_dropped_frames++;
        }
    }
}

static void infrared_rx_task(void* arg) {
    infrared_handle_t* self = (infrared_handle_t*)arg;

    while (!self->rx_task_stop) {
        if (xQueueReceive(self->rx_queue, &self->rx_dispatch_item, infrared_rx_task_poll_ticks(self)) != pdTRUE) {
            continue;
        }

        if (!self->cfg.on_receive) {
            continue;
        }

        const infrared_rx_frame_t frame = {
            .durations      = self->rx_dispatch_item.durations,
            .duration_count = self->rx_dispatch_item.duration_count,
            .overflowed     = self->rx_dispatch_item.overflowed,
        };

        self->cfg.on_receive(&frame, self->cfg.user_ctx);
    }

    if (self->rx_task_done) {
        xSemaphoreGive(self->rx_task_done);
    }

    vTaskDelete(NULL);
}

esp_err_t infrared_rx_init(infrared_handle_t* self) {
    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    if (self->rx_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    self->rx_queue = xQueueCreate(infrared_rx_queue_depth(self), sizeof(infrared_rx_queue_item_t));
    if (!self->rx_queue) {
        return ESP_ERR_NO_MEM;
    }

    self->rx_task_done = xSemaphoreCreateBinary();
    if (!self->rx_task_done) {
        infrared_rx_deinit(self);
        return ESP_ERR_NO_MEM;
    }

    self->rx_idle_timer = xTimerCreate(
        "infrared_idle",
        infrared_rx_idle_ticks(),
        pdFALSE,
        self,
        infrared_rx_idle_timer_cb
    );
    if (!self->rx_idle_timer) {
        infrared_rx_deinit(self);
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_ok = xTaskCreate(
        infrared_rx_task,
        INFRARED_RX_TASK_NAME,
        infrared_rx_task_stack_size(self),
        self,
        infrared_rx_task_priority(self),
        &self->rx_task
    );
    if (task_ok != pdPASS) {
        infrared_rx_deinit(self);
        return ESP_ERR_NO_MEM;
    }

    const gpio_config_t gpio_cfg = {
        .pin_bit_mask = 1ULL << self->cfg.rx_gpio,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };

    esp_err_t err = gpio_config(&gpio_cfg);
    if (err != ESP_OK) {
        infrared_rx_deinit(self);
        return err;
    }
    self->rx_gpio_configured = true;

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        infrared_rx_deinit(self);
        return err;
    }

    err = gpio_isr_handler_add(self->cfg.rx_gpio, infrared_rx_isr, self);
    if (err != ESP_OK) {
        infrared_rx_deinit(self);
        return err;
    }
    self->rx_isr_added = true;

    err = gpio_intr_enable(self->cfg.rx_gpio);
    if (err != ESP_OK) {
        infrared_rx_deinit(self);
        return err;
    }

    self->rx_initialized = true;
    return ESP_OK;
}

void infrared_rx_deinit(infrared_handle_t* self) {
    if (!self) {
        return;
    }

    if (self->rx_isr_added) {
        (void)gpio_intr_disable(self->cfg.rx_gpio);
        (void)gpio_isr_handler_remove(self->cfg.rx_gpio);
        self->rx_isr_added = false;
    }

    if (self->rx_idle_timer) {
        (void)xTimerStop(self->rx_idle_timer, 0);
        (void)xTimerDelete(self->rx_idle_timer, 0);
        self->rx_idle_timer = NULL;
    }

    if (self->rx_task) {
        self->rx_task_stop = true;
        if (self->rx_task_done) {
            (void)xSemaphoreTake(self->rx_task_done, pdMS_TO_TICKS(INFRARED_RX_TASK_STOP_WAIT_MS));
        }
        self->rx_task = NULL;
    }

    if (self->rx_task_done) {
        vSemaphoreDelete(self->rx_task_done);
        self->rx_task_done = NULL;
    }

    if (self->rx_queue) {
        vQueueDelete(self->rx_queue);
        self->rx_queue = NULL;
    }

    if (self->rx_gpio_configured) {
        (void)gpio_reset_pin(self->cfg.rx_gpio);
        self->rx_gpio_configured = false;
    }

    portENTER_CRITICAL(&self->rx_mux);
    infrared_rx_reset_frame_locked(self);
    portEXIT_CRITICAL(&self->rx_mux);

    self->rx_task_stop   = false;
    self->rx_initialized = false;
}
