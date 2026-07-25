#include "infrastructure/system/info/esp_impl.h"

#include <stdlib.h>
#include <string.h>

#include "domain/contracts/system/info.h"
#include "domain/models/error.h"
#include "domain/models/system.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "infrastructure/system/info/esp_impl_utils.h"

/* Contract Function Prototypes */

static dom_models_error_t get_project_info_impl(
    dom_contracts_system_info_t*      self,
    dom_models_system_project_info_t* out
);
static dom_models_error_t get_chip_info_impl(
    dom_contracts_system_info_t*   self,
    dom_models_system_chip_info_t* out
);

/* Constructor and Destructor */

dom_contracts_system_info_t* inf_system_info_esp_impl_new(const inf_system_info_esp_impl_cfg_t* cfg) {
    inf_system_info_esp_impl_ctx_t* ctx = (inf_system_info_esp_impl_ctx_t*)calloc(1, sizeof(inf_system_info_esp_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    inf_system_info_esp_impl_cfg_t default_cfg = INF_SYSTEM_INFO_ESP_IMPL_CFG_DEFAULT();
    memcpy(&ctx->cfg, cfg ? cfg : &default_cfg, sizeof(inf_system_info_esp_impl_cfg_t));
    if (!ctx->cfg.project_name) {
        ctx->cfg.project_name = default_cfg.project_name;
    }
    if (!ctx->cfg.project_version) {
        ctx->cfg.project_version = default_cfg.project_version;
    }
    if (!ctx->cfg.name) {
        ctx->cfg.name = default_cfg.name;
    }
    if (!ctx->cfg.type) {
        ctx->cfg.type = default_cfg.type;
    }
    if (!ctx->cfg.firmware_version) {
        ctx->cfg.firmware_version = default_cfg.firmware_version;
    }

    dom_contracts_system_info_t* self = dom_contracts_system_info_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->get_project_info = get_project_info_impl;
    self->get_chip_info    = get_chip_info_impl;

    return self;
}

void inf_system_info_esp_impl_delete(dom_contracts_system_info_t* self) {
    if (!self) {
        return;
    }

    free(self->ctx);
    dom_contracts_system_info_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t get_project_info_impl(
    dom_contracts_system_info_t*      self,
    dom_models_system_project_info_t* out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_system_info_esp_impl_ctx_t* ctx = self->ctx;

    memset(out, 0, sizeof(dom_models_system_project_info_t));

    inf_system_info_esp_impl_copy_cstr(out->project_name, sizeof(out->project_name), ctx->cfg.project_name);
    inf_system_info_esp_impl_copy_cstr(out->project_version, sizeof(out->project_version), ctx->cfg.project_version);
    inf_system_info_esp_impl_copy_cstr(out->name, sizeof(out->name), ctx->cfg.name);
    inf_system_info_esp_impl_copy_cstr(out->type, sizeof(out->type), ctx->cfg.type);
    inf_system_info_esp_impl_copy_cstr(out->firmware_version, sizeof(out->firmware_version), ctx->cfg.firmware_version);

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_chip_info_impl(
    dom_contracts_system_info_t*   self,
    dom_models_system_chip_info_t* out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(dom_models_system_chip_info_t));

    uint8_t   mac[6];
    esp_err_t esp_err = esp_base_mac_addr_get(mac);
    if (esp_err != ESP_OK) {
        return inf_system_info_esp_impl_error_from_esp(esp_err);
    }

    dom_models_error_t err = inf_system_info_esp_impl_format_mac(mac, out->hardware_mac, sizeof(out->hardware_mac));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    esp_chip_info_t chip_info;
    memset(&chip_info, 0, sizeof(esp_chip_info_t));
    esp_chip_info(&chip_info);

    inf_system_info_esp_impl_copy_cstr(out->model, sizeof(out->model), inf_system_info_esp_impl_chip_model_str(chip_info.model));
    out->revision = chip_info.revision;
    out->cores    = chip_info.cores;

    return DOMAIN_MODELS_ERROR_OK;
}
