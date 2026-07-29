#include "presentation/mqtt/context_utils.h"

dom_models_error_t pres_mqtt_context_validate_cfg(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota
) {
    if (!logger || !preloaded_repository || !messaging_callbacks || !settings || !ota) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
