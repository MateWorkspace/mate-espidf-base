#include "infrastructure/repository/preloaded/nvs_impl.h"

#include <stdlib.h>
#include <string.h>

#include "domain/contracts/repository/preloaded.h"
#include "domain/models/preloaded.h"
#include "infrastructure/repository/preloaded/nvs_impl_types.h"
#include "infrastructure/repository/preloaded/nvs_impl_utils.h"

/* Helper Function Prototypes */

static dom_models_error_t get_string(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size,
    const char*                           value
);
static dom_models_error_t set_string(
    dom_contracts_repository_preloaded_t* self,
    const char*                           key,
    char**                                runtime_value,
    const char*                           value
);

/* Contract Function Prototypes */

static dom_models_error_t get_device_id_impl(
    dom_contracts_repository_preloaded_t* self,
    uint64_t*                             out
);
static dom_models_error_t get_device_id_str_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
);
static dom_models_error_t get_mqtt_proto_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
);
static dom_models_error_t set_mqtt_proto_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
);
static dom_models_error_t get_mqtt_host_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
);
static dom_models_error_t set_mqtt_host_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
);
static dom_models_error_t get_mqtt_port_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
);
static dom_models_error_t set_mqtt_port_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
);
static dom_models_error_t get_mqtt_user_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
);
static dom_models_error_t set_mqtt_user_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
);
static dom_models_error_t get_mqtt_pass_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
);
static dom_models_error_t set_mqtt_pass_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
);
static dom_models_error_t get_system_restart_after_ms_impl(
    dom_contracts_repository_preloaded_t* self,
    uint32_t*                             out
);
static dom_models_error_t set_system_restart_after_ms_impl(
    dom_contracts_repository_preloaded_t* self,
    uint32_t                              value
);
static dom_models_error_t get_wifi_sta_auto_reconnect_impl(
    dom_contracts_repository_preloaded_t* self,
    bool*                                 out
);
static dom_models_error_t set_wifi_sta_auto_reconnect_impl(
    dom_contracts_repository_preloaded_t* self,
    bool                                  value
);

/* Constructor and Destructor */

dom_contracts_repository_preloaded_t* inf_repository_preloaded_nvs_impl_new(const inf_repository_preloaded_nvs_impl_cfg_t* cfg) {
    if (!cfg || !cfg->nvs) {
        return NULL;
    }

    inf_repository_preloaded_nvs_impl_ctx_t* ctx = (inf_repository_preloaded_nvs_impl_ctx_t*)calloc(1, sizeof(inf_repository_preloaded_nvs_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    memcpy(&ctx->cfg, cfg, sizeof(inf_repository_preloaded_nvs_impl_cfg_t));

    dom_contracts_repository_preloaded_t* self = dom_contracts_repository_preloaded_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->get_device_id               = get_device_id_impl;
    self->get_device_id_str           = get_device_id_str_impl;
    self->get_mqtt_proto              = get_mqtt_proto_impl;
    self->set_mqtt_proto              = set_mqtt_proto_impl;
    self->get_mqtt_host               = get_mqtt_host_impl;
    self->set_mqtt_host               = set_mqtt_host_impl;
    self->get_mqtt_port               = get_mqtt_port_impl;
    self->set_mqtt_port               = set_mqtt_port_impl;
    self->get_mqtt_user               = get_mqtt_user_impl;
    self->set_mqtt_user               = set_mqtt_user_impl;
    self->get_mqtt_pass               = get_mqtt_pass_impl;
    self->set_mqtt_pass               = set_mqtt_pass_impl;
    self->get_system_restart_after_ms = get_system_restart_after_ms_impl;
    self->set_system_restart_after_ms = set_system_restart_after_ms_impl;
    self->get_wifi_sta_auto_reconnect = get_wifi_sta_auto_reconnect_impl;
    self->set_wifi_sta_auto_reconnect = set_wifi_sta_auto_reconnect_impl;

    return self;
}

void inf_repository_preloaded_nvs_impl_delete(dom_contracts_repository_preloaded_t* self) {
    if (!self) {
        return;
    }

    free(self->ctx);
    dom_contracts_repository_preloaded_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t get_device_id_impl(
    dom_contracts_repository_preloaded_t* self,
    uint64_t*                             out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = dom_models_preloaded_data.device_id;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_device_id_str_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    return get_string(self, out, out_size, dom_models_preloaded_data.device_id_str);
}

static dom_models_error_t get_mqtt_proto_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    return get_string(self, out, out_size, dom_models_preloaded_data.mqtt_proto);
}

static dom_models_error_t set_mqtt_proto_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    return set_string(self, DOMAIN_MODELS_PRELOADED_MQTT_PROTO_KEY, &dom_models_preloaded_data.mqtt_proto, value);
}

static dom_models_error_t get_mqtt_host_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    return get_string(self, out, out_size, dom_models_preloaded_data.mqtt_host);
}

static dom_models_error_t set_mqtt_host_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    return set_string(self, DOMAIN_MODELS_PRELOADED_MQTT_HOST_KEY, &dom_models_preloaded_data.mqtt_host, value);
}

static dom_models_error_t get_mqtt_port_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    return get_string(self, out, out_size, dom_models_preloaded_data.mqtt_port);
}

static dom_models_error_t set_mqtt_port_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    return set_string(self, DOMAIN_MODELS_PRELOADED_MQTT_PORT_KEY, &dom_models_preloaded_data.mqtt_port, value);
}

static dom_models_error_t get_mqtt_user_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    return get_string(self, out, out_size, dom_models_preloaded_data.mqtt_user);
}

static dom_models_error_t set_mqtt_user_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    return set_string(self, DOMAIN_MODELS_PRELOADED_MQTT_USER_KEY, &dom_models_preloaded_data.mqtt_user, value);
}

static dom_models_error_t get_mqtt_pass_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    return get_string(self, out, out_size, dom_models_preloaded_data.mqtt_pass);
}

static dom_models_error_t set_mqtt_pass_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    return set_string(self, DOMAIN_MODELS_PRELOADED_MQTT_PASS_KEY, &dom_models_preloaded_data.mqtt_pass, value);
}

/* Helper Function Implementations */

static dom_models_error_t get_string(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size,
    const char*                           value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_repository_preloaded_nvs_impl_copy_cstr(out, out_size, value);
}

static dom_models_error_t set_string(
    dom_contracts_repository_preloaded_t* self,
    const char*                           key,
    char**                                runtime_value,
    const char*                           value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_nvs_impl_ctx_t* ctx = self->ctx;

    return inf_repository_preloaded_nvs_impl_set_string(ctx->cfg.nvs, key, runtime_value, value);
}

static dom_models_error_t get_system_restart_after_ms_impl(
    dom_contracts_repository_preloaded_t* self,
    uint32_t*                             out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = dom_models_preloaded_data.system_restart_after_ms;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t set_system_restart_after_ms_impl(
    dom_contracts_repository_preloaded_t* self,
    uint32_t                              value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_nvs_impl_ctx_t* ctx = self->ctx;

    esp_err_t err = nvs_set_u32(ctx->cfg.nvs, DOMAIN_MODELS_PRELOADED_SYSTEM_RESTART_AFTER_MS_KEY, value);
    if (err != ESP_OK) {
        return inf_repository_preloaded_nvs_impl_error_from_esp(err);
    }

    err = nvs_commit(ctx->cfg.nvs);
    if (err != ESP_OK) {
        return inf_repository_preloaded_nvs_impl_error_from_esp(err);
    }

    dom_models_preloaded_data.system_restart_after_ms = value;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_wifi_sta_auto_reconnect_impl(
    dom_contracts_repository_preloaded_t* self,
    bool*                                 out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    *out = dom_models_preloaded_data.wifi_sta_auto_reconnect;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t set_wifi_sta_auto_reconnect_impl(
    dom_contracts_repository_preloaded_t* self,
    bool                                  value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_nvs_impl_ctx_t* ctx = self->ctx;

    esp_err_t err = nvs_set_u8(ctx->cfg.nvs, DOMAIN_MODELS_PRELOADED_WIFI_STA_AUTO_RECONNECT_KEY, value ? 1 : 0);
    if (err != ESP_OK) {
        return inf_repository_preloaded_nvs_impl_error_from_esp(err);
    }

    err = nvs_commit(ctx->cfg.nvs);
    if (err != ESP_OK) {
        return inf_repository_preloaded_nvs_impl_error_from_esp(err);
    }

    dom_models_preloaded_data.wifi_sta_auto_reconnect = value;

    return DOMAIN_MODELS_ERROR_OK;
}
