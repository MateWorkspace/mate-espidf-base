#include "presentation/ble/handler/settings/handler.h"

#include <stdlib.h>
#include <string.h>

#include "domain/models/error.h"
#include "host/ble_att.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "presentation/ble/gatt/util.h"
#include "presentation/ble/gatt/uuid.h"
#include "presentation/ble/handler/settings/dto.h"

#define BASE_TAG "ble/handler/settings"

/* Global-lifetime GATT storage - NimBLE retains raw pointers into these
   forever once ble_gatts_add_svcs() runs (see gatt/registry.h), so they
   cannot be stack- or heap-owned by a struct that might be freed. There is
   only ever one settings handler instance in this app, so file-scope
   static storage (rather than something allocated per handler instance) is
   both correct and the simplest option that satisfies that constraint. */
static struct ble_gatt_chr_def characteristic_defs[5];
static struct ble_gatt_svc_def service_defs[2];

static int data_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int update_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int restart_required_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int restart_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);

pres_ble_handler_settings_t* pres_ble_handler_settings_new(const pres_ble_handler_settings_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->settings || !cfg->gatt_registry) {
        return NULL;
    }

    pres_ble_handler_settings_t* self = (pres_ble_handler_settings_t*)calloc(1, sizeof(pres_ble_handler_settings_t));
    if (!self) {
        return NULL;
    }

    memcpy(&self->cfg, cfg, sizeof(pres_ble_handler_settings_cfg_t));

    return self;
}

void pres_ble_handler_settings_delete(pres_ble_handler_settings_t* self) {
    if (!self) {
        return;
    }

    pres_ble_handler_settings_deinit(self);
    free(self);
}

dom_models_error_t pres_ble_handler_settings_init(pres_ble_handler_settings_t* self) {
    const char* tag = BASE_TAG "/init";

    if (!self) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->registered) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    characteristic_defs[0] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_settings_data_chr.u,
        .access_cb = data_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_READ,
    };
    characteristic_defs[1] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_settings_update_chr.u,
        .access_cb = update_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_WRITE,
    };
    characteristic_defs[2] = (struct ble_gatt_chr_def){
        .uuid       = &pres_ble_gatt_uuid_settings_restart_required_chr.u,
        .access_cb  = restart_required_access_callback,
        .arg        = self,
        .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &self->restart_required_chr_hdl,
    };
    characteristic_defs[3] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_settings_restart_chr.u,
        .access_cb = restart_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_WRITE,
    };
    characteristic_defs[4] = (struct ble_gatt_chr_def){0};

    service_defs[0] = (struct ble_gatt_svc_def){
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &pres_ble_gatt_uuid_settings_service.u,
        .characteristics = characteristic_defs,
    };
    service_defs[1] = (struct ble_gatt_svc_def){0};

    dom_models_error_t err = pres_ble_gatt_registry_add_service(self->cfg.gatt_registry, &service_defs[0]);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to add settings GATT service: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    self->registered = true;

    self->cfg.logger->info(self->cfg.logger, tag, "Settings BLE handler initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_ble_handler_settings_deinit(pres_ble_handler_settings_t* self) {
    if (!self || !self->registered) {
        return;
    }

    for (size_t i = 0; i < 4; i++) {
        characteristic_defs[i].access_cb = pres_ble_gatt_util_disabled_access_callback;
        characteristic_defs[i].arg       = NULL;
    }

    self->registered = false;
}

static int data_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_settings_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    dom_usecases_internal_settings_snapshot_t snapshot;
    dom_models_error_t                        err = self->cfg.settings->get_snapshot(self->cfg.settings, &snapshot);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t json_len = pres_ble_handler_settings_dto_encode_snapshot(&snapshot, self->snapshot_json, sizeof(self->snapshot_json));
    if (json_len == 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return pres_ble_gatt_util_write_read_response(ctxt, self->snapshot_json, json_len);
}

static int update_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_settings_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    size_t payload_len = 0;
    int    rc          = pres_ble_gatt_util_read_write_payload(ctxt, self->update_payload, sizeof(self->update_payload), &payload_len);
    if (rc != 0) {
        return rc;
    }

    dom_usecases_internal_settings_preloaded_update_t update;
    dom_models_error_t                                err = pres_ble_handler_settings_dto_decode_update(self->update_payload, payload_len, &update);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    bool restart_required = false;
    err                   = self->cfg.settings->set_preloaded(self->cfg.settings, &update, &restart_required);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (self->restart_required_chr_hdl != 0) {
        ble_gatts_chr_updated(self->restart_required_chr_hdl);
    }

    return 0;
}

static int restart_required_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_settings_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    bool               restart_required = false;
    dom_models_error_t err              = self->cfg.settings->get_restart_required(self->cfg.settings, &restart_required);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t json_len = pres_ble_handler_settings_dto_encode_restart_required(restart_required, self->restart_required_json, sizeof(self->restart_required_json));
    if (json_len == 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return pres_ble_gatt_util_write_read_response(ctxt, self->restart_required_json, json_len);
}

static int restart_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_settings_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    uint32_t delay_ms = 0;
    if (OS_MBUF_PKTLEN(ctxt->om) < sizeof(delay_ms)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    int rc = ble_hs_mbuf_to_flat(ctxt->om, &delay_ms, sizeof(delay_ms), NULL);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    dom_models_error_t err = self->cfg.settings->restart(self->cfg.settings, delay_ms);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}
