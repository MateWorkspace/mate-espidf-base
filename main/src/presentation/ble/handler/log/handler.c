#include "presentation/ble/handler/log/handler.h"

#include <stdlib.h>
#include <string.h>

#include "domain/models/error.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "presentation/ble/gatt/util.h"
#include "presentation/ble/gatt/uuid.h"
#include "presentation/ble/handler/log/utils.h"

#define BASE_TAG "ble/handler/log"

/* Lifetime BLE Definitions */

static struct ble_gatt_chr_def characteristic_defs[3];
static struct ble_gatt_svc_def service_defs[2];

/* Access Callback Function Prototypes */

static int message_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int enabled_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);

static void on_log_message(void* cb_ctx, const char* msg, size_t msg_len);
static void on_gap_event(void* cb_ctx, const struct ble_gap_event* event);
static void log_task(void* arg);

/* Constructors and Destructors */

pres_ble_handler_log_t* pres_ble_handler_log_new(const pres_ble_handler_log_cfg_t* cfg) {
    if (pres_ble_handler_log_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    pres_ble_handler_log_t* self = (pres_ble_handler_log_t*)calloc(1, sizeof(pres_ble_handler_log_t));
    if (!self) {
        return NULL;
    }

    memcpy(&self->cfg, cfg, sizeof(pres_ble_handler_log_cfg_t));

    return self;
}

void pres_ble_handler_log_delete(pres_ble_handler_log_t* self) {
    if (!self) {
        return;
    }

    pres_ble_handler_log_deinit(self);
    free(self);
}

dom_models_error_t pres_ble_handler_log_init(pres_ble_handler_log_t* self) {
    const char* tag = BASE_TAG "/init";

    if (!self) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->registered) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    characteristic_defs[0] = (struct ble_gatt_chr_def){
        .uuid       = &pres_ble_gatt_uuid_log_message_chr.u,
        .access_cb  = message_access_callback,
        .arg        = self,
        .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &self->message_chr_hdl,
    };
    characteristic_defs[1] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_log_enabled_chr.u,
        .access_cb = enabled_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
    };
    characteristic_defs[2] = (struct ble_gatt_chr_def){0};

    service_defs[0] = (struct ble_gatt_svc_def){
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &pres_ble_gatt_uuid_log_service.u,
        .characteristics = characteristic_defs,
    };
    service_defs[1] = (struct ble_gatt_svc_def){0};

    dom_models_error_t err = pres_ble_gatt_registry_add_service(self->cfg.gatt_registry, &service_defs[0]);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to add log GATT service: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    self->queue = xQueueCreate(PRES_BLE_HANDLER_LOG_QUEUE_LEN, sizeof(pres_ble_handler_log_queue_item_t));
    if (!self->queue) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to create log queue: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    BaseType_t task_result = xTaskCreate(
        log_task,
        PRES_BLE_HANDLER_LOG_TASK_NAME,
        PRES_BLE_HANDLER_LOG_TASK_STACK_SIZE,
        self,
        PRES_BLE_HANDLER_LOG_TASK_PRIORITY,
        &self->task_handle
    );
    if (task_result != pdPASS) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to start log task: %s (%d)", dom_models_error_str(DOMAIN_MODELS_ERROR_MALLOC_FAILED), (int)DOMAIN_MODELS_ERROR_MALLOC_FAILED);
        vQueueDelete(self->queue);
        self->queue = NULL;
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    err = pres_ble_host_add_gap_event_callback(self->cfg.host, self, on_gap_event);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to subscribe to BLE GAP events: %s (%d)", dom_models_error_str(err), (int)err);
        vTaskDelete(self->task_handle);
        self->task_handle = NULL;
        vQueueDelete(self->queue);
        self->queue = NULL;
        return err;
    }
    self->gap_cb_subscribed = true;

    err = self->cfg.log_forwarding->add_sink(self->cfg.log_forwarding, self, on_log_message);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to subscribe to log forwarding: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }
    self->logger_cb_subscribed = true;

    self->registered = true;

    self->cfg.logger->info(self->cfg.logger, tag, "Log BLE handler initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_ble_handler_log_deinit(pres_ble_handler_log_t* self) {
    if (!self) {
        return;
    }

    if (self->logger_cb_subscribed) {
        self->cfg.log_forwarding->remove_sink(self->cfg.log_forwarding, on_log_message);
        self->logger_cb_subscribed = false;
    }

    if (self->gap_cb_subscribed) {
        pres_ble_host_remove_gap_event_callback(self->cfg.host, on_gap_event);
        self->gap_cb_subscribed = false;
    }

    self->enabled    = false;
    self->subscribed = false;

    if (self->task_handle) {
        vTaskDelete(self->task_handle);
        self->task_handle = NULL;
    }
    if (self->queue) {
        vQueueDelete(self->queue);
        self->queue = NULL;
    }

    if (!self->registered) {
        return;
    }

    for (size_t i = 0; i < 2; i++) {
        characteristic_defs[i].access_cb = pres_ble_gatt_util_disabled_access_callback;
        characteristic_defs[i].arg       = NULL;
    }

    self->registered = false;
}

/* Access Callback Function Implementations */

static void on_log_message(void* cb_ctx, const char* msg, size_t msg_len) {
    pres_ble_handler_log_t* self = cb_ctx;
    if (!self || !msg || msg_len == 0) {
        return;
    }

    if (!self->enabled || !self->subscribed) {
        return;
    }

    pres_ble_handler_log_queue_item_t item;
    size_t                            copy_len = msg_len < (sizeof(item.message) - 1) ? msg_len : (sizeof(item.message) - 1);
    memcpy(item.message, msg, copy_len);
    item.message[copy_len] = '\0';
    item.message_len       = copy_len;

    /* Non-blocking: a full queue means the ble_log task is behind, and this
       line is silently dropped rather than stalling the caller's task. */
    (void)xQueueSend(self->queue, &item, 0);
}

static void log_task(void* arg) {
    pres_ble_handler_log_t* self = arg;
    if (!self) {
        vTaskDelete(NULL);
        return;
    }

    pres_ble_handler_log_queue_item_t item;
    for (;;) {
        if (xQueueReceive(self->queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        memcpy(self->message, item.message, item.message_len);
        self->message[item.message_len] = '\0';
        self->message_len               = item.message_len;

        if (self->message_chr_hdl != 0) {
            ble_gatts_chr_updated(self->message_chr_hdl);
        }
    }
}

static void on_gap_event(void* cb_ctx, const struct ble_gap_event* event) {
    pres_ble_handler_log_t* self = cb_ctx;
    if (!self || !event) {
        return;
    }

    switch (event->type) {
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (self->message_chr_hdl != 0 && event->subscribe.attr_handle == self->message_chr_hdl) {
                self->subscribed = event->subscribe.cur_notify != 0;
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            /* Not every central sends an explicit unsubscribe before
               disconnecting - reset defensively so a stale "subscribed"
               flag can't keep the pipeline gated in forever. */
            self->subscribed = false;
            break;

        default:
            break;
    }
}

static int message_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_log_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    return pres_ble_gatt_util_write_read_response(ctxt, self->message, self->message_len);
}

static int enabled_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_log_t* self = arg;
    if (!self || !ctxt) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t value = self->enabled ? 1 : 0;
        return pres_ble_gatt_util_write_read_response(ctxt, (const char*)&value, sizeof(value));
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t value = 0;
        if (OS_MBUF_PKTLEN(ctxt->om) < sizeof(value)) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        int rc = ble_hs_mbuf_to_flat(ctxt->om, &value, sizeof(value), NULL);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }

        self->enabled = value != 0;
        return 0;
    }

    return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
}
