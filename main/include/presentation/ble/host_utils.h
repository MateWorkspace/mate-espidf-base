#ifndef PRESENTATION_BLE_HOST_UTILS_H
#define PRESENTATION_BLE_HOST_UTILS_H

#include "domain/models/error.h"
#include "presentation/ble/host.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_ble_host_validate_cfg(const pres_ble_host_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_HOST_UTILS_H */
