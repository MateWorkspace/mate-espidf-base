#ifndef PRESENTATION_MQTT_CONTEXT_UTILS_H
#define PRESENTATION_MQTT_CONTEXT_UTILS_H

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/messaging/def_pub.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/models/error.h"
#include "domain/usecases/internal/messaging_callbacks.h"
#include "domain/usecases/internal/ota.h"
#include "domain/usecases/internal/settings.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t pres_mqtt_context_validate_cfg(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota,
    dom_contracts_messaging_def_pub_t*           def_pub
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_CONTEXT_UTILS_H */
