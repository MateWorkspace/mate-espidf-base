#include "presentation/mqtt/handler/config/handler.h"

#include <stdlib.h>
#include <string.h>

#include "domain/models/error.h"
#include "domain/models/preloaded.h"
#include "presentation/mqtt/handler/config/dto.h"

#define BASE_TAG "pres_mqtt_config"

void pres_mqtt_handler_config(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->info(ctx->logger, tag, "Received config request via MQTT");

    pres_mqtt_handler_config_dto_request_t request;
    dom_models_error_t                     err = pres_mqtt_handler_config_dto_decode(data, data_len, &request);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, tag, "Failed to parse config payload: %s (%d)", dom_models_error_str(err), (int)err);
        return;
    }

    if (!request.key_set || !request.value_set) {
        ctx->logger->error(ctx->logger, tag, "Invalid config payload: missing key or value field");
        return;
    }

    const dom_models_preloaded_schema_entry_t* matched_entry = NULL;
    for (size_t i = 0; i < DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT; i++) {
        if (strcmp(dom_models_preloaded_schema[i].key, request.key) == 0) {
            matched_entry = &dom_models_preloaded_schema[i];
            break;
        }
    }
    if (!matched_entry) {
        ctx->logger->warn(ctx->logger, tag, "Unknown config key: %s", request.key);
        return;
    }

    dom_usecases_internal_settings_preloaded_update_t update;
    memset(&update, 0, sizeof(update));

    /* Type interpretation comes from the schema entry (the same source of truth
       BLE's config_schema characteristic and upload.py consume); the key name
       only selects which update field to populate. */
    bool handled = false;

    switch (matched_entry->type) {
        case DOMAIN_MODELS_PRELOADED_VALUE_TYPE_STRING: {
            if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_PROTO_KEY) == 0) {
                strncpy(update.mqtt_proto, request.value, sizeof(update.mqtt_proto) - 1);
                update.mqtt_proto_set = true;
                handled               = true;
            } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_HOST_KEY) == 0) {
                strncpy(update.mqtt_host, request.value, sizeof(update.mqtt_host) - 1);
                update.mqtt_host_set = true;
                handled              = true;
            } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_PORT_KEY) == 0) {
                strncpy(update.mqtt_port, request.value, sizeof(update.mqtt_port) - 1);
                update.mqtt_port_set = true;
                handled              = true;
            } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_USER_KEY) == 0) {
                strncpy(update.mqtt_user, request.value, sizeof(update.mqtt_user) - 1);
                update.mqtt_user_set = true;
                handled              = true;
            } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_PASS_KEY) == 0) {
                strncpy(update.mqtt_pass, request.value, sizeof(update.mqtt_pass) - 1);
                update.mqtt_pass_set = true;
                handled              = true;
            }
            break;
        }

        case DOMAIN_MODELS_PRELOADED_VALUE_TYPE_UINT32: {
            char*         endptr = NULL;
            unsigned long value  = strtoul(request.value, &endptr, 10);
            /* strtoul("") leaves endptr at the terminating null, so the empty
               string has to be rejected explicitly. */
            if (request.value[0] == '\0' || !endptr || *endptr != '\0') {
                ctx->logger->warn(ctx->logger, tag, "Invalid config value for %s: %s", request.key, request.value);
                return;
            }

            if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_SYSTEM_RESTART_AFTER_MS_KEY) == 0) {
                /* 0 would make the boot-time restart fire with no delay on every
                   boot, an unrecoverable boot loop. Reject it here. */
                if (value == 0) {
                    ctx->logger->warn(ctx->logger, tag, "Invalid config value for %s: %s", request.key, request.value);
                    return;
                }
                update.system_restart_after_ms     = (uint32_t)value;
                update.system_restart_after_ms_set = true;
                handled                            = true;
            }
            break;
        }

        case DOMAIN_MODELS_PRELOADED_VALUE_TYPE_BOOL: {
            if (strcmp(request.value, "true") != 0 && strcmp(request.value, "false") != 0) {
                ctx->logger->warn(ctx->logger, tag, "Invalid config value for %s: %s", request.key, request.value);
                return;
            }

            if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_WIFI_STA_TRY_CONNECT_ON_INIT_KEY) == 0) {
                update.wifi_try_init     = strcmp(request.value, "true") == 0;
                update.wifi_try_init_set = true;
                handled                  = true;
            }
            break;
        }
    }

    if (!handled) {
        ctx->logger->error(ctx->logger, tag, "Config key %s has no MQTT handler mapping despite being in the schema", request.key);
        return;
    }

    bool               restart_required = false;
    dom_models_error_t set_err          = ctx->settings->set_preloaded(ctx->settings, &update, &restart_required);
    if (set_err == DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->info(ctx->logger, tag, "Config value updated successfully via MQTT: %s", request.key);
    }
    /* No error log on failure here - the usecase (settings' set_preloaded_impl)
       already logs the failure; re-logging the same condition here would
       double-log it (matches the action handler's restart-path precedent). */
}
