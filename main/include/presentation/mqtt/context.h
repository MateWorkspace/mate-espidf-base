#ifndef PRESENTATION_MQTT_CONTEXT_H
#define PRESENTATION_MQTT_CONTEXT_H

#include <stdbool.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/contracts/repository/preloaded.h"
#include "domain/usecases/internal/messaging_callbacks.h"
#include "domain/usecases/internal/ota.h"
#include "domain/usecases/internal/settings.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_MQTT_CONTEXT_TOPIC_MAX_LEN 128

typedef struct {
    dom_contracts_logger_leveled_t*              logger;
    dom_contracts_repository_preloaded_t*        preloaded_repository;
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks;
    dom_usecases_internal_ota_t*                 ota;
    dom_usecases_internal_settings_t*            settings;
    esp_mqtt_client_handle_t                     mqtt_client;
    char                                         device_id_str[37];
    bool                                         registered;
    /* Struct-owned (not stack-local in on_message.c's callback, which runs
       on esp-mqtt's internal event task) - same rationale as
       presentation/ble/handler/settings/types.h's comment: a stack-local
       buffer in a callback on a task with limited stack already caused a
       real crash this session (see docs/agent_test/v1.0.0-dev.1/scenario/09-known-gaps-summary.md,
       bugs #6/#7); this fixes the one remaining presentation handler that
       still had the same pattern before it had a chance to crash the
       same way. */
    char                                         topic_scratch[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
    char                                         registration_ack_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
    char                                         ota_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
    char                                         action_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
    char                                         config_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
} pres_mqtt_context_t;

pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota
);

void pres_mqtt_context_delete(pres_mqtt_context_t* self);

dom_models_error_t pres_mqtt_context_init(pres_mqtt_context_t* self, esp_mqtt_client_handle_t mqtt_client);

void pres_mqtt_context_deinit(pres_mqtt_context_t* self);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_CONTEXT_H */
