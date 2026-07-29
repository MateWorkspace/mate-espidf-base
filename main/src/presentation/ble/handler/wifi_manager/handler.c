#include "presentation/ble/handler/wifi_manager/handler.h"

#include <stdlib.h>
#include <string.h>

#include "domain/models/error.h"
#include "host/ble_att.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "presentation/ble/gatt/util.h"
#include "presentation/ble/gatt/uuid.h"
#include "presentation/ble/handler/wifi_manager/dto.h"

#define BASE_TAG "ble/handler/wifi_manager"

/* Global-lifetime GATT storage, same rationale as the settings handler
   (see its handler.c) - NimBLE retains raw pointers into these forever
   once registered, and there is only ever one wifi_manager BLE handler
   instance in this app. */
static struct ble_gatt_chr_def characteristic_defs[6];
static struct ble_gatt_svc_def service_defs[2];

static int status_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int connect_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int command_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int stored_credential_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int try_connect_on_init_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);

static void on_wifi_status_event(void* cb_ctx, const dom_models_wifi_event_t* event);

pres_ble_handler_wifi_manager_t* pres_ble_handler_wifi_manager_new(const pres_ble_handler_wifi_manager_cfg_t* cfg) {
    if (!cfg || !cfg->logger || !cfg->wifi_manager || !cfg->gatt_registry) {
        return NULL;
    }

    pres_ble_handler_wifi_manager_t* self = (pres_ble_handler_wifi_manager_t*)calloc(1, sizeof(pres_ble_handler_wifi_manager_t));
    if (!self) {
        return NULL;
    }

    memcpy(&self->cfg, cfg, sizeof(pres_ble_handler_wifi_manager_cfg_t));

    return self;
}

void pres_ble_handler_wifi_manager_delete(pres_ble_handler_wifi_manager_t* self) {
    if (!self) {
        return;
    }

    pres_ble_handler_wifi_manager_deinit(self);
    free(self);
}

dom_models_error_t pres_ble_handler_wifi_manager_init(pres_ble_handler_wifi_manager_t* self) {
    const char* tag = BASE_TAG "/init";

    if (!self) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->registered) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    characteristic_defs[0] = (struct ble_gatt_chr_def){
        .uuid       = &pres_ble_gatt_uuid_wifi_status_chr.u,
        .access_cb  = status_access_callback,
        .arg        = self,
        .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &self->status_chr_hdl,
    };
    characteristic_defs[1] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_wifi_connect_chr.u,
        .access_cb = connect_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_WRITE,
    };
    characteristic_defs[2] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_wifi_command_chr.u,
        .access_cb = command_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_WRITE,
    };
    characteristic_defs[3] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_wifi_stored_credential_chr.u,
        .access_cb = stored_credential_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_READ,
    };
    characteristic_defs[4] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_wifi_try_connect_on_init_chr.u,
        .access_cb = try_connect_on_init_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
    };
    characteristic_defs[5] = (struct ble_gatt_chr_def){0};

    service_defs[0] = (struct ble_gatt_svc_def){
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &pres_ble_gatt_uuid_wifi_service.u,
        .characteristics = characteristic_defs,
    };
    service_defs[1] = (struct ble_gatt_svc_def){0};

    dom_models_error_t err = pres_ble_gatt_registry_add_service(self->cfg.gatt_registry, &service_defs[0]);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to add wifi_manager GATT service: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    err = self->cfg.wifi_manager->add_status_callback(self->cfg.wifi_manager, self, on_wifi_status_event);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to subscribe to WiFi status events: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }
    self->status_cb_subscribed = true;

    self->registered = true;

    self->cfg.logger->info(self->cfg.logger, tag, "WiFi manager BLE handler initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_ble_handler_wifi_manager_deinit(pres_ble_handler_wifi_manager_t* self) {
    if (!self) {
        return;
    }

    if (self->status_cb_subscribed) {
        self->cfg.wifi_manager->remove_status_callback(self->cfg.wifi_manager, on_wifi_status_event);
        self->status_cb_subscribed = false;
    }

    if (!self->registered) {
        return;
    }

    for (size_t i = 0; i < 5; i++) {
        characteristic_defs[i].access_cb = pres_ble_gatt_util_disabled_access_callback;
        characteristic_defs[i].arg       = NULL;
    }

    self->registered = false;
}

static void on_wifi_status_event(void* cb_ctx, const dom_models_wifi_event_t* event) {
    (void)event;

    pres_ble_handler_wifi_manager_t* self = cb_ctx;
    if (!self || self->status_chr_hdl == 0) {
        return;
    }

    /* Notify just tells NimBLE "the value changed" - it re-invokes
       status_access_callback itself to fetch the current value for the
       notification payload, so there's nothing to format/send here. */
    ble_gatts_chr_updated(self->status_chr_hdl);
}

static int status_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_wifi_manager_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    dom_usecases_internal_wifi_manager_status_t status;
    dom_models_error_t                          err = self->cfg.wifi_manager->get_status(self->cfg.wifi_manager, &status);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t json_len = pres_ble_handler_wifi_manager_dto_encode_status(&status, self->status_json, sizeof(self->status_json));
    if (json_len == 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return pres_ble_gatt_util_write_read_response(ctxt, self->status_json, json_len);
}

static int connect_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_wifi_manager_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    size_t payload_len = 0;
    int    rc          = pres_ble_gatt_util_read_write_payload(ctxt, self->connect_payload, sizeof(self->connect_payload), &payload_len);
    if (rc != 0) {
        return rc;
    }

    dom_models_wifi_sta_connect_config_t credential;
    dom_models_error_t                   err = pres_ble_handler_wifi_manager_dto_decode_connect(self->connect_payload, payload_len, &credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    err = self->cfg.wifi_manager->connect(self->cfg.wifi_manager, &credential);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

static int command_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_wifi_manager_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    uint8_t opcode = 0;
    if (OS_MBUF_PKTLEN(ctxt->om) < sizeof(opcode)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    int rc = ble_hs_mbuf_to_flat(ctxt->om, &opcode, sizeof(opcode), NULL);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    dom_models_error_t err = DOMAIN_MODELS_ERROR_OK;
    switch ((pres_ble_handler_wifi_manager_command_t)opcode) {
        case PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_STOP:
            err = self->cfg.wifi_manager->stop(self->cfg.wifi_manager);
            break;
        case PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_START:
            err = self->cfg.wifi_manager->start(self->cfg.wifi_manager);
            break;
        case PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_CONNECT_STORED:
            err = self->cfg.wifi_manager->connect_stored(self->cfg.wifi_manager);
            break;
        case PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_DISCONNECT:
            err = self->cfg.wifi_manager->disconnect(self->cfg.wifi_manager);
            break;
        case PRES_BLE_HANDLER_WIFI_MANAGER_COMMAND_FORGET_STORED:
            err = self->cfg.wifi_manager->forget_stored_credential(self->cfg.wifi_manager);
            break;
        default:
            return BLE_ATT_ERR_UNLIKELY;
    }

    return err == DOMAIN_MODELS_ERROR_OK ? 0 : BLE_ATT_ERR_UNLIKELY;
}

static int stored_credential_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_wifi_manager_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    dom_usecases_internal_wifi_manager_stored_sta_t stored;
    dom_models_error_t                              err = self->cfg.wifi_manager->get_stored_credential(self->cfg.wifi_manager, &stored);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t json_len = pres_ble_handler_wifi_manager_dto_encode_stored_credential(&stored, self->stored_credential_json, sizeof(self->stored_credential_json));
    if (json_len == 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return pres_ble_gatt_util_write_read_response(ctxt, self->stored_credential_json, json_len);
}

static int try_connect_on_init_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_wifi_manager_t* self = arg;
    if (!self || !ctxt) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        bool               enabled = false;
        dom_models_error_t err     = self->cfg.wifi_manager->get_try_connect_on_init(self->cfg.wifi_manager, &enabled);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            return BLE_ATT_ERR_UNLIKELY;
        }

        uint8_t value = enabled ? 1 : 0;
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

        dom_models_error_t err = self->cfg.wifi_manager->set_try_connect_on_init(self->cfg.wifi_manager, value != 0);
        return err == DOMAIN_MODELS_ERROR_OK ? 0 : BLE_ATT_ERR_UNLIKELY;
    }

    return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
}
