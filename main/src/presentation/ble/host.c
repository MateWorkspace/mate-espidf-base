#include "presentation/ble/host.h"

#include <stdlib.h>
#include <string.h>

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_id.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "presentation/ble/host_utils.h"
#include "services/gap/ble_svc_gap.h"

#define BASE_TAG "ble/host"

/* NimBLE's ble_hs_cfg.reset_cb/sync_cb and the ble_gap_adv_start() event
   callback are plain function pointers with no (or a NimBLE-owned) context
   argument - there is exactly one BLE host per device, so a single static
   "active instance" pointer is what NimBLE's own API shape requires, the
   same reason idf-base's host.c uses static state instead of threading a
   ctx pointer through these particular callbacks. */
static pres_ble_host_t* active_host;
static uint8_t          own_addr_type;

static int  gap_event(struct ble_gap_event* event, void* arg);
static int  start_advertising(void);
static void on_reset(int reason);
static void on_sync(void);
static void host_task(void* param);

pres_ble_host_t* pres_ble_host_new(const pres_ble_host_cfg_t* cfg) {
    if (pres_ble_host_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    pres_ble_host_t* self = (pres_ble_host_t*)calloc(1, sizeof(pres_ble_host_t));
    if (!self) {
        return NULL;
    }

    memcpy(&self->cfg, cfg, sizeof(pres_ble_host_cfg_t));

    return self;
}

void pres_ble_host_delete(pres_ble_host_t* self) {
    if (!self) {
        return;
    }

    pres_ble_host_stop(self);
    free(self);
}

dom_models_error_t pres_ble_host_add_gap_event_callback(
    pres_ble_host_t*           self,
    void*                      cb_ctx,
    pres_ble_host_gap_event_cb cb_func
) {
    if (!self || !cb_func) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->gap_event_cb_idx >= PRES_BLE_HOST_GAP_EVENT_CB_MAX_CNT) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    self->gap_event_cb_funcs[self->gap_event_cb_idx] = cb_func;
    self->gap_event_cb_ctxs[self->gap_event_cb_idx]  = cb_ctx;
    self->gap_event_cb_idx += 1;

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_ble_host_remove_gap_event_callback(
    pres_ble_host_t*           self,
    pres_ble_host_gap_event_cb cb_func
) {
    if (!self || !cb_func || self->gap_event_cb_idx == 0) {
        return;
    }

    for (unsigned int i = 0; i < self->gap_event_cb_idx; i++) {
        if (self->gap_event_cb_funcs[i] != cb_func) {
            continue;
        }

        unsigned int last_idx = self->gap_event_cb_idx - 1;

        self->gap_event_cb_funcs[i] = NULL;
        self->gap_event_cb_ctxs[i]  = NULL;

        if (i != last_idx) {
            self->gap_event_cb_funcs[i] = self->gap_event_cb_funcs[last_idx];
            self->gap_event_cb_ctxs[i]  = self->gap_event_cb_ctxs[last_idx];

            self->gap_event_cb_funcs[last_idx] = NULL;
            self->gap_event_cb_ctxs[last_idx]  = NULL;
        }

        self->gap_event_cb_idx -= 1;
        return;
    }
}

dom_models_error_t pres_ble_host_start(pres_ble_host_t* self) {
    const char* tag = BASE_TAG "/start";

    if (!self) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->started) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    dom_models_error_t err = pres_ble_gatt_registry_register_all(self->cfg.gatt_registry);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to register GATT services: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    active_host = self;

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb  = on_sync;

    nimble_port_freertos_init(host_task);
    self->started = true;

    self->cfg.logger->info(self->cfg.logger, tag, "BLE host starting as %s", self->cfg.device_name);

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_ble_host_stop(pres_ble_host_t* self) {
    if (!self || !self->started) {
        return;
    }

    const char* tag = BASE_TAG "/stop";

    if (ble_gap_adv_active()) {
        int rc = ble_gap_adv_stop();
        if (rc != 0 && rc != BLE_HS_EALREADY) {
            self->cfg.logger->warn(self->cfg.logger, tag, "Failed to stop BLE advertising: %d", rc);
        }
    }

    int rc = nimble_port_stop();
    if (rc != 0) {
        self->cfg.logger->warn(self->cfg.logger, tag, "Failed to stop NimBLE host: %d", rc);
    }

    if (active_host == self) {
        active_host = NULL;
    }
    self->started = false;
}

static int start_advertising(void) {
    const char* tag = BASE_TAG "/start_advertising";

    if (!active_host || ble_gap_adv_active()) {
        return 0;
    }

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    const char* name = ble_svc_gap_device_name();
    if (name) {
        fields.name             = (const uint8_t*)name;
        fields.name_len         = strlen(name);
        fields.name_is_complete = 1;
    }

    /* Three 128-bit service UUIDs (settings, wifi_manager, log) don't fit
       in a ~31-byte advertising packet, so this only advertises the device
       name; clients discover services after connecting via standard GATT
       discovery, same limitation idf-base's advertising had. */
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        active_host->cfg.logger->error(active_host->cfg.logger, tag, "Failed to set advertising fields: %d", rc);
        return rc;
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);
    if (rc != 0) {
        active_host->cfg.logger->error(active_host->cfg.logger, tag, "Failed to start advertising: %d", rc);
        return rc;
    }

    active_host->cfg.logger->info(active_host->cfg.logger, tag, "BLE advertising started");

    return 0;
}

static int gap_event(struct ble_gap_event* event, void* arg) {
    const char* tag = BASE_TAG "/gap_event";

    (void)arg;

    if (!event || !active_host) {
        return 0;
    }

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                active_host->cfg.logger->info(active_host->cfg.logger, tag, "BLE client connected");
            } else {
                active_host->cfg.logger->warn(active_host->cfg.logger, tag, "BLE connection attempt failed: %d", event->connect.status);
                (void)start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            active_host->cfg.logger->info(active_host->cfg.logger, tag, "BLE client disconnected: %d", event->disconnect.reason);
            (void)start_advertising();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            (void)start_advertising();
            break;

        default:
            break;
    }

    for (unsigned int i = 0; i < active_host->gap_event_cb_idx; i++) {
        if (active_host->gap_event_cb_funcs[i]) {
            active_host->gap_event_cb_funcs[i](active_host->gap_event_cb_ctxs[i], event);
        }
    }

    return 0;
}

static void on_reset(int reason) {
    const char* tag = BASE_TAG "/on_reset";

    if (active_host) {
        active_host->cfg.logger->error(active_host->cfg.logger, tag, "NimBLE reset: %d", reason);
    }
}

static void on_sync(void) {
    const char* tag = BASE_TAG "/on_sync";

    if (!active_host) {
        return;
    }

    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        active_host->cfg.logger->error(active_host->cfg.logger, tag, "Failed to ensure BLE address: %d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        active_host->cfg.logger->error(active_host->cfg.logger, tag, "Failed to infer BLE address type: %d", rc);
        return;
    }

    (void)start_advertising();
}

static void host_task(void* param) {
    (void)param;

    nimble_port_run();
    nimble_port_freertos_deinit();
}
