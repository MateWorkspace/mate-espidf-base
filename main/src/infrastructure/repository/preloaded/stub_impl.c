#include "infrastructure/repository/preloaded/stub_impl.h"

#include <stdlib.h>

#include "domain/contracts/repository/preloaded.h"
#include "infrastructure/repository/preloaded/stub_impl_types.h"
#include "infrastructure/repository/preloaded/stub_impl_utils.h"

/* Helper Function Prototypes */

static dom_models_error_t get_string(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size,
    const char*                           value
);
static dom_models_error_t set_string(
    dom_contracts_repository_preloaded_t* self,
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

/* Constructor and Destructor */

dom_contracts_repository_preloaded_t* inf_repository_preloaded_stub_impl_new(const inf_repository_preloaded_stub_impl_cfg_t* cfg) {
    inf_repository_preloaded_stub_impl_ctx_t* ctx = (inf_repository_preloaded_stub_impl_ctx_t*)calloc(1, sizeof(inf_repository_preloaded_stub_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    inf_repository_preloaded_stub_impl_cfg_t default_cfg = INF_REPOSITORY_PRELOADED_STUB_IMPL_CFG_DEFAULT();
    dom_models_error_t                       err         = inf_repository_preloaded_stub_impl_load_cfg(ctx, cfg ? cfg : &default_cfg);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        free(ctx);
        return NULL;
    }

    dom_contracts_repository_preloaded_t* self = dom_contracts_repository_preloaded_new(ctx);
    if (!self) {
        inf_repository_preloaded_stub_impl_clear(ctx);
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

    return self;
}

void inf_repository_preloaded_stub_impl_delete(dom_contracts_repository_preloaded_t* self) {
    if (!self) {
        return;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        inf_repository_preloaded_stub_impl_clear(ctx);
        free(ctx);
    }

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

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;
    *out                                          = ctx->device_id;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t get_device_id_str_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return get_string(self, out, out_size, ctx->device_id_str);
}

static dom_models_error_t get_mqtt_proto_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return get_string(self, out, out_size, ctx->mqtt_proto);
}

static dom_models_error_t set_mqtt_proto_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return set_string(self, &ctx->mqtt_proto, value);
}

static dom_models_error_t get_mqtt_host_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return get_string(self, out, out_size, ctx->mqtt_host);
}

static dom_models_error_t set_mqtt_host_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return set_string(self, &ctx->mqtt_host, value);
}

static dom_models_error_t get_mqtt_port_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return get_string(self, out, out_size, ctx->mqtt_port);
}

static dom_models_error_t set_mqtt_port_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return set_string(self, &ctx->mqtt_port, value);
}

static dom_models_error_t get_mqtt_user_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return get_string(self, out, out_size, ctx->mqtt_user);
}

static dom_models_error_t set_mqtt_user_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return set_string(self, &ctx->mqtt_user, value);
}

static dom_models_error_t get_mqtt_pass_impl(
    dom_contracts_repository_preloaded_t* self,
    char*                                 out,
    size_t                                out_size
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return get_string(self, out, out_size, ctx->mqtt_pass);
}

static dom_models_error_t set_mqtt_pass_impl(
    dom_contracts_repository_preloaded_t* self,
    const char*                           value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;

    return set_string(self, &ctx->mqtt_pass, value);
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

    return inf_repository_preloaded_stub_impl_copy_cstr(out, out_size, value);
}

static dom_models_error_t set_string(
    dom_contracts_repository_preloaded_t* self,
    char**                                runtime_value,
    const char*                           value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_repository_preloaded_stub_impl_set_string(runtime_value, value);
}

static dom_models_error_t get_system_restart_after_ms_impl(
    dom_contracts_repository_preloaded_t* self,
    uint32_t*                             out
) {
    if (!self || !self->ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;
    *out                                          = ctx->system_restart_after_ms;

    return DOMAIN_MODELS_ERROR_OK;
}

static dom_models_error_t set_system_restart_after_ms_impl(
    dom_contracts_repository_preloaded_t* self,
    uint32_t                              value
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_repository_preloaded_stub_impl_ctx_t* ctx = self->ctx;
    ctx->system_restart_after_ms                  = value;

    return DOMAIN_MODELS_ERROR_OK;
}
