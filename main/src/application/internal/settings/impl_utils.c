#include "application/internal/settings/impl_utils.h"

#include <string.h>

#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/system/restart.h"

/* Helper Function Prototypes */

static bool has_preloaded_repository_functions(dom_contracts_repository_preloaded_t* preloaded_repository);
static bool has_system_restart_functions(dom_contracts_system_restart_t* system_restart);

dom_models_error_t app_internal_settings_impl_validate_cfg(const app_internal_settings_impl_cfg_t* cfg) {
    if (!cfg ||
        !cfg->logger ||
        !cfg->logger->error ||
        !cfg->logger->info ||
        !has_preloaded_repository_functions(cfg->preloaded_repository) ||
        !has_system_restart_functions(cfg->system_restart)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_settings_impl_load_snapshot(
    app_internal_settings_impl_ctx_t*          ctx,
    dom_usecases_internal_settings_snapshot_t* out
) {
    if (!ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(dom_usecases_internal_settings_snapshot_t));

    dom_models_error_t err = ctx->cfg.preloaded_repository->get_device_id(ctx->cfg.preloaded_repository, &out->device_id);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.preloaded_repository->get_device_id_str(ctx->cfg.preloaded_repository, out->device_id_str, sizeof(out->device_id_str));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.preloaded_repository->get_mqtt_proto(ctx->cfg.preloaded_repository, out->mqtt_proto, sizeof(out->mqtt_proto));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.preloaded_repository->get_mqtt_host(ctx->cfg.preloaded_repository, out->mqtt_host, sizeof(out->mqtt_host));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.preloaded_repository->get_mqtt_port(ctx->cfg.preloaded_repository, out->mqtt_port, sizeof(out->mqtt_port));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.preloaded_repository->get_mqtt_user(ctx->cfg.preloaded_repository, out->mqtt_user, sizeof(out->mqtt_user));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.preloaded_repository->get_mqtt_pass(ctx->cfg.preloaded_repository, out->mqtt_pass, sizeof(out->mqtt_pass));
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = ctx->cfg.preloaded_repository->get_system_restart_after_ms(ctx->cfg.preloaded_repository, &out->system_restart_after_ms);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

bool app_internal_settings_impl_has_preloaded_update(const dom_usecases_internal_settings_preloaded_update_t* update) {
    return update &&
           (update->mqtt_proto_set ||
            update->mqtt_host_set ||
            update->mqtt_port_set ||
            update->mqtt_user_set ||
            update->mqtt_pass_set ||
            update->system_restart_after_ms_set);
}

/* Helper Function Implementations */

static bool has_preloaded_repository_functions(dom_contracts_repository_preloaded_t* preloaded_repository) {
    return preloaded_repository &&
           preloaded_repository->get_device_id &&
           preloaded_repository->get_device_id_str &&
           preloaded_repository->get_mqtt_proto &&
           preloaded_repository->set_mqtt_proto &&
           preloaded_repository->get_mqtt_host &&
           preloaded_repository->set_mqtt_host &&
           preloaded_repository->get_mqtt_port &&
           preloaded_repository->set_mqtt_port &&
           preloaded_repository->get_mqtt_user &&
           preloaded_repository->set_mqtt_user &&
           preloaded_repository->get_mqtt_pass &&
           preloaded_repository->set_mqtt_pass &&
           preloaded_repository->get_system_restart_after_ms &&
           preloaded_repository->set_system_restart_after_ms;
}

static bool has_system_restart_functions(dom_contracts_system_restart_t* system_restart) {
    return system_restart &&
           system_restart->restart;
}
