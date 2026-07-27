#ifndef PRESENTATION_BLE_HOST_H
#define PRESENTATION_BLE_HOST_H

#include <stdbool.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/models/error.h"
#include "host/ble_gap.h"
#include "presentation/ble/gatt/registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_BLE_HOST_GAP_EVENT_CB_MAX_CNT 4

/* Fired for every NimBLE GAP event (connect, disconnect, subscribe, ...).
   Feature handlers that care about connection/subscription state (the log
   handler is the only one right now, gating its notify pipeline on the
   Log-Message characteristic's subscribe state) register here instead of
   the host needing to know which characteristic handle belongs to which
   feature - each callback self-filters by inspecting event fields such as
   event->subscribe.attr_handle against its own val_handle. */
typedef void (*pres_ble_host_gap_event_cb)(void* cb_ctx, const struct ble_gap_event* event);

typedef struct {
    dom_contracts_logger_leveled_t* logger;
    pres_ble_gatt_registry_t*       gatt_registry;
    char                            device_name[40];
} pres_ble_host_cfg_t;

typedef struct {
    pres_ble_host_cfg_t        cfg;
    bool                       started;
    pres_ble_host_gap_event_cb gap_event_cb_funcs[PRES_BLE_HOST_GAP_EVENT_CB_MAX_CNT];
    void*                      gap_event_cb_ctxs[PRES_BLE_HOST_GAP_EVENT_CB_MAX_CNT];
    unsigned int               gap_event_cb_idx;
} pres_ble_host_t;

pres_ble_host_t* pres_ble_host_new(const pres_ble_host_cfg_t* cfg);

void pres_ble_host_delete(pres_ble_host_t* self);

dom_models_error_t pres_ble_host_add_gap_event_callback(
    pres_ble_host_t*           self,
    void*                      cb_ctx,
    pres_ble_host_gap_event_cb cb_func
);

void pres_ble_host_remove_gap_event_callback(
    pres_ble_host_t*           self,
    pres_ble_host_gap_event_cb cb_func
);

/* Registers every service accumulated in cfg.gatt_registry (must be called
   after every feature handler has added its service - NimBLE requires all
   ble_gatts_add_svcs() calls to complete before the host syncs/runs), then
   starts the NimBLE host FreeRTOS task and, once synced, advertising. */
dom_models_error_t pres_ble_host_start(pres_ble_host_t* self);

void pres_ble_host_stop(pres_ble_host_t* self);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HOST_H */
