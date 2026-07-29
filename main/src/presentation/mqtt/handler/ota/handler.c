#include "presentation/mqtt/handler/ota/handler.h"

#include "domain/models/error.h"
#include "domain/models/update.h"
#include "presentation/mqtt/handler/ota/dto.h"

#define BASE_TAG "pres_mqtt_ota"

void pres_mqtt_handler_ota(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->info(ctx->logger, tag, "Received OTA update trigger via MQTT");

    dom_models_update_info_t update_info;
    dom_models_error_t       err = pres_mqtt_handler_ota_dto_decode(data, data_len, &update_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, tag, "Failed to parse OTA payload: %s (%d)", dom_models_error_str(err), (int)err);
        return;
    }

    err = ctx->ota->update(ctx->ota, &update_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        /* No error log on failure here - the usecase (ota's update_impl)
           already logs the failure; see Phase 6. */
        return;
    }

    ctx->logger->info(ctx->logger, tag, "OTA update requested for URL: %s", update_info.firmware_url);
}
