#include "presentation/ble/handler/system_info/handler.h"

#include <stdlib.h>
#include <string.h>

#include "domain/models/error.h"
#include "host/ble_att.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "presentation/ble/gatt/util.h"
#include "presentation/ble/gatt/uuid.h"
#include "presentation/ble/handler/system_info/dto.h"
#include "presentation/ble/handler/system_info/utils.h"

#define BASE_TAG "ble/handler/system_info"

/* Lifetime BLE Definitions */

static struct ble_gatt_chr_def characteristic_defs[3];
static struct ble_gatt_svc_def service_defs[2];

/* Access Callback Function Prototypes */

static int info_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
static int config_schema_access_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);

/* Constructors and Destructors */

pres_ble_handler_system_info_t* pres_ble_handler_system_info_new(const pres_ble_handler_system_info_cfg_t* cfg) {
    if (pres_ble_handler_system_info_validate_cfg(cfg) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    pres_ble_handler_system_info_t* self = (pres_ble_handler_system_info_t*)calloc(1, sizeof(pres_ble_handler_system_info_t));
    if (!self) {
        return NULL;
    }

    memcpy(&self->cfg, cfg, sizeof(pres_ble_handler_system_info_cfg_t));

    return self;
}

void pres_ble_handler_system_info_delete(pres_ble_handler_system_info_t* self) {
    if (!self) {
        return;
    }

    pres_ble_handler_system_info_deinit(self);
    free(self);
}

dom_models_error_t pres_ble_handler_system_info_init(pres_ble_handler_system_info_t* self) {
    const char* tag = BASE_TAG "/init";

    if (!self) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->registered) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    characteristic_defs[0] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_system_info_info_chr.u,
        .access_cb = info_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_READ,
    };
    characteristic_defs[1] = (struct ble_gatt_chr_def){
        .uuid      = &pres_ble_gatt_uuid_system_info_config_schema_chr.u,
        .access_cb = config_schema_access_callback,
        .arg       = self,
        .flags     = BLE_GATT_CHR_F_READ,
    };
    characteristic_defs[2] = (struct ble_gatt_chr_def){0};

    service_defs[0] = (struct ble_gatt_svc_def){
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &pres_ble_gatt_uuid_system_info_service.u,
        .characteristics = characteristic_defs,
    };
    service_defs[1] = (struct ble_gatt_svc_def){0};

    dom_models_error_t err = pres_ble_gatt_registry_add_service(self->cfg.gatt_registry, &service_defs[0]);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        self->cfg.logger->error(self->cfg.logger, tag, "Failed to add system info GATT service: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }

    self->registered = true;

    self->cfg.logger->info(self->cfg.logger, tag, "System Info BLE handler initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_ble_handler_system_info_deinit(pres_ble_handler_system_info_t* self) {
    if (!self || !self->registered) {
        return;
    }

    for (size_t i = 0; i < 2; i++) {
        characteristic_defs[i].access_cb = pres_ble_gatt_util_disabled_access_callback;
        characteristic_defs[i].arg       = NULL;
    }

    self->registered = false;
}

/* Access Callback Function Implementations */

static int info_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_system_info_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    dom_models_system_project_info_t project;
    dom_models_error_t               err = self->cfg.system_info->get_project_info(self->cfg.system_info, &project);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    dom_models_system_chip_info_t chip;
    err = self->cfg.system_info->get_chip_info(self->cfg.system_info, &chip);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t json_len = pres_ble_handler_system_info_dto_encode_info(&project, &chip, self->info_json, sizeof(self->info_json));
    if (json_len == 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return pres_ble_gatt_util_write_read_response(ctxt, self->info_json, json_len);
}

static int config_schema_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;

    pres_ble_handler_system_info_t* self = arg;
    if (!self || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    const dom_models_preloaded_schema_entry_t* entries     = NULL;
    size_t                                     entry_count = 0;
    dom_models_error_t                         err         = self->cfg.system_info->get_preloaded_schema(self->cfg.system_info, &entries, &entry_count);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t json_len = pres_ble_handler_system_info_dto_encode_config_schema(entries, entry_count, self->config_schema_json, sizeof(self->config_schema_json));
    if (json_len == 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return pres_ble_gatt_util_write_read_response(ctxt, self->config_schema_json, json_len);
}
