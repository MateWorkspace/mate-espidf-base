#include "presentation/task/wifi_sta_reconnect/utils.h"

#include <string.h>

#include "domain/models/error.h"
#include "presentation/task/wifi_sta_reconnect/types.h"

dom_models_error_t pres_task_wifi_sta_reconnect_validate_cfg(
    const pres_task_wifi_sta_reconnect_cfg_t* cfg
) {
    if (!cfg ||
        !cfg->wifi_manager ||
        !cfg->wifi_manager->need_reconnect ||
        !cfg->wifi_manager->try_reconnect) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_task_wifi_sta_reconnect_normalize_cfg(
    pres_task_wifi_sta_reconnect_cfg_t*       out,
    const pres_task_wifi_sta_reconnect_cfg_t* cfg
) {
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(pres_task_wifi_sta_reconnect_cfg_t));
    if (!cfg) {
        return;
    }

    memcpy(out, cfg, sizeof(pres_task_wifi_sta_reconnect_cfg_t));

    if (!out->task_name || out->task_name[0] == '\0') {
        out->task_name = PRES_TASK_WIFI_STA_RECONNECT_DEFAULT_TASK_NAME;
    }
    if (out->stack_size == 0) {
        out->stack_size = PRES_TASK_WIFI_STA_RECONNECT_DEFAULT_STACK_SIZE;
    }
    if (out->priority == 0) {
        out->priority = PRES_TASK_WIFI_STA_RECONNECT_DEFAULT_PRIORITY;
    }
    if (out->interval_ms == 0) {
        out->interval_ms = PRES_TASK_WIFI_STA_RECONNECT_DEFAULT_INTERVAL_MS;
    }
}
