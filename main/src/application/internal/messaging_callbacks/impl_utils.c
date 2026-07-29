#include "application/internal/messaging_callbacks/impl_utils.h"

#include <stdbool.h>
#include <stdio.h>

#include "domain/contracts/messaging/def_pub.h"
#include "domain/contracts/messaging/def_sub.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/contracts/system/info.h"
#include "domain/contracts/system/restart.h"

/* Helper Function Prototypes */

static bool has_def_pub_functions(dom_contracts_messaging_def_pub_t* def_pub);
static bool has_def_sub_functions(dom_contracts_messaging_def_sub_t* def_sub);
static bool has_system_restart_functions(dom_contracts_system_restart_t* system_restart);
static bool has_preloaded_repository_functions(dom_contracts_repository_preloaded_t* preloaded_repository);
static bool has_system_info_functions(dom_contracts_system_info_t* system_info);

dom_models_error_t app_internal_messaging_callbacks_impl_validate_cfg(const app_internal_messaging_callbacks_impl_cfg_t* cfg) {
    if (!cfg ||
        !cfg->logger ||
        !cfg->logger->error ||
        !cfg->logger->info ||
        !has_def_pub_functions(cfg->def_pub) ||
        !has_def_sub_functions(cfg->def_sub) ||
        !has_system_restart_functions(cfg->system_restart) ||
        !has_preloaded_repository_functions(cfg->preloaded_repository) ||
        !has_system_info_functions(cfg->system_info) ||
        !cfg->log_forwarding ||
        !cfg->log_forwarding->add_sink ||
        !cfg->log_forwarding->remove_sink) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_messaging_callbacks_impl_build_firmware_name(
    const dom_models_system_project_info_t* project_info,
    char*                                   out,
    size_t                                  out_size
) {
    if (!project_info || !out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int written = snprintf(out, out_size, "%s_%s", project_info->project_name, project_info->project_version);
    if (written <= 0 || (size_t)written >= out_size) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t app_internal_messaging_callbacks_impl_build_device_info(
    const dom_models_system_chip_info_t* chip_info,
    char*                                out,
    size_t                               out_size
) {
    if (!chip_info || !out || out_size == 0) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    int written = snprintf(out, out_size, "%s,%s,%d,%d", chip_info->hardware_mac, chip_info->model, chip_info->revision, chip_info->cores);
    if (written <= 0 || (size_t)written >= out_size) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static bool has_def_pub_functions(dom_contracts_messaging_def_pub_t* def_pub) {
    return def_pub &&
           def_pub->is_connected &&
           def_pub->registration &&
           def_pub->status &&
           def_pub->log;
}

static bool has_def_sub_functions(dom_contracts_messaging_def_sub_t* def_sub) {
    return def_sub &&
           def_sub->registration_ack &&
           def_sub->ota &&
           def_sub->action;
}

static bool has_system_restart_functions(dom_contracts_system_restart_t* system_restart) {
    return system_restart &&
           system_restart->restart;
}

static bool has_preloaded_repository_functions(dom_contracts_repository_preloaded_t* preloaded_repository) {
    return preloaded_repository &&
           preloaded_repository->get_device_id_str;
}

static bool has_system_info_functions(dom_contracts_system_info_t* system_info) {
    return system_info &&
           system_info->get_project_info &&
           system_info->get_chip_info;
}
