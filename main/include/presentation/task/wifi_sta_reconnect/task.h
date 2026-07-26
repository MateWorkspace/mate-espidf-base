#ifndef PRESENTATION_TASK_WIFI_STA_RECONNECT_TASK_H
#define PRESENTATION_TASK_WIFI_STA_RECONNECT_TASK_H

#include "domain/models/error.h"
#include "presentation/task/wifi_sta_reconnect/types.h"

#ifdef __cplusplus
extern "C" {
#endif

pres_task_wifi_sta_reconnect_t* pres_task_wifi_sta_reconnect_new(
    const pres_task_wifi_sta_reconnect_cfg_t* cfg
);

void pres_task_wifi_sta_reconnect_delete(
    pres_task_wifi_sta_reconnect_t* self
);

dom_models_error_t pres_task_wifi_sta_reconnect_start(
    pres_task_wifi_sta_reconnect_t* self
);

dom_models_error_t pres_task_wifi_sta_reconnect_stop(
    pres_task_wifi_sta_reconnect_t* self
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_TASK_WIFI_STA_RECONNECT_TASK_H */
