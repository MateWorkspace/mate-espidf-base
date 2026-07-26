#include "application/internal/wifi_manager/impl_utils.h"

#include <stdbool.h>
#include <string.h>

#include "domain/contracts/device/wifi.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/repository/wifi.h"

/* Helper Function Prototypes */

static bool cstr_available(const char* value);
static bool has_wifi_functions(dom_contracts_device_wifi_t* wifi);
static bool has_wifi_repository_functions(dom_contracts_repository_wifi_t* wifi_repository);
static bool has_preloaded_repository_functions(dom_contracts_repository_preloaded_t* preloaded_repository);

dom_models_error_t app_internal_wifi_manager_impl_normalize_cfg(app_internal_wifi_manager_impl_cfg_t* cfg) {
    if (!cfg) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    if (cfg->reconnect_max_trials == 0) {
        cfg->reconnect_max_trials = APP_INTERNAL_WIFI_MANAGER_IMPL_DEFAULT_RECONNECT_MAX_TRIALS;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_wifi_manager_impl_validate_cfg(const app_internal_wifi_manager_impl_cfg_t* cfg) {
    if (!cfg ||
        !cfg->logger ||
        !cfg->logger->error ||
        !cfg->logger->info ||
        !has_wifi_functions(cfg->wifi) ||
        !has_wifi_repository_functions(cfg->wifi_repository) ||
        !has_preloaded_repository_functions(cfg->preloaded_repository)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_wifi_manager_impl_validate_credential(const dom_models_wifi_sta_connect_config_t* credential) {
    if (!credential || !cstr_available(credential->ssid)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_wifi_manager_impl_load_stored_sta(
    app_internal_wifi_manager_impl_ctx_t*            ctx,
    dom_usecases_internal_wifi_manager_stored_sta_t* out
) {
    if (!ctx || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(dom_usecases_internal_wifi_manager_stored_sta_t));

    dom_models_wifi_sta_connect_config_t credential;
    dom_models_error_t                   err = ctx->cfg.wifi_repository->get_sta_credential(ctx->cfg.wifi_repository, &credential);
    if (err == DOMAIN_MODELS_ERROR_NOT_FOUND) {
        out->available = false;
        return DOMAIN_MODELS_ERROR_OK;
    }
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    out->available = true;
    strncpy(out->ssid, credential.ssid, sizeof(out->ssid) - 1);

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static bool cstr_available(const char* value) {
    return value && value[0] != '\0';
}

static bool has_wifi_functions(dom_contracts_device_wifi_t* wifi) {
    return wifi &&
           wifi->start &&
           wifi->stop &&
           wifi->get_status &&
           wifi->connect_sta &&
           wifi->disconnect_sta;
}

static bool has_wifi_repository_functions(dom_contracts_repository_wifi_t* wifi_repository) {
    return wifi_repository &&
           wifi_repository->get_sta_credential &&
           wifi_repository->set_sta_credential &&
           wifi_repository->clear_sta_credential;
}

static bool has_preloaded_repository_functions(dom_contracts_repository_preloaded_t* preloaded_repository) {
    return preloaded_repository &&
           preloaded_repository->get_wifi_sta_try_connect_on_init &&
           preloaded_repository->set_wifi_sta_try_connect_on_init;
}
