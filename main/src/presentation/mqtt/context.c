#include "presentation/mqtt/context.h"

#include <stdlib.h>

#include "domain/models/error.h"

pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_ota_t*                 ota
) {
    if (!logger || !preloaded_repository || !messaging_callbacks || !ota) {
        return NULL;
    }

    pres_mqtt_context_t* self = (pres_mqtt_context_t*)calloc(1, sizeof(pres_mqtt_context_t));
    if (!self) {
        return NULL;
    }

    self->logger               = logger;
    self->preloaded_repository = preloaded_repository;
    self->messaging_callbacks  = messaging_callbacks;
    self->ota                  = ota;

    dom_models_error_t err = preloaded_repository->get_device_id_str(
        preloaded_repository,
        self->device_id_str,
        sizeof(self->device_id_str)
    );
    if (err != DOMAIN_MODELS_ERROR_OK) {
        free(self);
        return NULL;
    }

    return self;
}

void pres_mqtt_context_delete(pres_mqtt_context_t* self) {
    if (!self) {
        return;
    }

    free(self);
}
