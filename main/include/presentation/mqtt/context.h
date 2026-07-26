#ifndef PRESENTATION_MQTT_CONTEXT_H
#define PRESENTATION_MQTT_CONTEXT_H

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/usecases/internal/messaging_callbacks.h"
#include "domain/usecases/internal/ota.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dom_contracts_logger_leveled_t*              logger;
    dom_contracts_repository_preloaded_t*        preloaded_repository;
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks;
    dom_usecases_internal_ota_t*                 ota;
    char                                         device_id_str[37];
} pres_mqtt_context_t;

pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_ota_t*                  ota
);

void pres_mqtt_context_delete(pres_mqtt_context_t* self);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_CONTEXT_H */
