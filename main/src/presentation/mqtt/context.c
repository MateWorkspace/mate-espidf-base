#include "presentation/mqtt/context.h"

#include <stdio.h>
#include <stdlib.h>

#include "domain/models/error.h"
#include "presentation/mqtt/context_utils.h"
#include "presentation/mqtt/event/event_handler.h"

#define BASE_TAG "pres_mqtt_context"

pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_ota_t*                 ota
) {
    if (pres_mqtt_context_validate_cfg(logger, preloaded_repository, messaging_callbacks, ota) != DOMAIN_MODELS_ERROR_OK) {
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

    logger->info(logger, BASE_TAG "/new", "MQTT context created successfully");

    return self;
}

void pres_mqtt_context_delete(pres_mqtt_context_t* self) {
    if (!self) {
        return;
    }

    free(self);
}

dom_models_error_t pres_mqtt_context_init(pres_mqtt_context_t* self, esp_mqtt_client_handle_t mqtt_client) {
    const char* tag = BASE_TAG "/init";

    if (!self || !mqtt_client) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    if (self->registered) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    int written = snprintf(self->registration_ack_topic, sizeof(self->registration_ack_topic), "/sub/%s/registration_ack", self->device_id_str);
    if (written <= 0 || (size_t)written >= sizeof(self->registration_ack_topic)) {
        self->logger->error(self->logger, tag, "Failed to build registration_ack topic string");
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    written = snprintf(self->ota_topic, sizeof(self->ota_topic), "/sub/%s/ota", self->device_id_str);
    if (written <= 0 || (size_t)written >= sizeof(self->ota_topic)) {
        self->logger->error(self->logger, tag, "Failed to build ota topic string");
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    written = snprintf(self->action_topic, sizeof(self->action_topic), "/sub/%s/action", self->device_id_str);
    if (written <= 0 || (size_t)written >= sizeof(self->action_topic)) {
        self->logger->error(self->logger, tag, "Failed to build action topic string");
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    esp_err_t esp_err = esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        pres_mqtt_event_handler,
        self
    );
    if (esp_err != ESP_OK) {
        self->logger->error(self->logger, tag, "Failed to register MQTT event handler: %d", (int)esp_err);
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    self->registered = true;

    self->logger->info(self->logger, tag, "MQTT context initialized successfully");

    return DOMAIN_MODELS_ERROR_OK;
}

void pres_mqtt_context_deinit(pres_mqtt_context_t* self, esp_mqtt_client_handle_t mqtt_client) {
    if (!self || !mqtt_client || !self->registered) {
        return;
    }

    esp_mqtt_client_unregister_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        pres_mqtt_event_handler
    );

    self->registered = false;
}
