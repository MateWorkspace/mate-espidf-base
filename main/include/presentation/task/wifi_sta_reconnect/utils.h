#ifndef PRESENTATION_TASK_WIFI_STA_RECONNECT_UTILS_H
#define PRESENTATION_TASK_WIFI_STA_RECONNECT_UTILS_H

#include "domain/models/error.h"
#include "presentation/task/wifi_sta_reconnect/types.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_task_wifi_sta_reconnect_validate_cfg(
    const pres_task_wifi_sta_reconnect_cfg_t* cfg
);

void pres_task_wifi_sta_reconnect_normalize_cfg(
    pres_task_wifi_sta_reconnect_cfg_t*       out,
    const pres_task_wifi_sta_reconnect_cfg_t* cfg
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_TASK_WIFI_STA_RECONNECT_UTILS_H */
