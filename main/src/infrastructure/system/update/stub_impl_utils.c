#include "infrastructure/system/update/stub_impl_utils.h"

#include <ctype.h>
#include <string.h>

/* Helper Function Prototypes */

static bool sha256_checksum_valid(const char* checksum);

dom_models_error_t inf_system_update_stub_impl_load_cfg(
    inf_system_update_stub_impl_ctx_t*       ctx,
    const inf_system_update_stub_impl_cfg_t* cfg
) {
    if (!ctx || !cfg) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(ctx, 0, sizeof(inf_system_update_stub_impl_ctx_t));
    ctx->update_result   = cfg->update_result;
    ctx->validate_result = cfg->validate_result;
    ctx->rollback_result = cfg->rollback_result;

    if (cfg->update_available) {
        dom_models_error_t err = inf_system_update_stub_impl_set_update(ctx, &cfg->update_info);
        if (err != DOMAIN_MODELS_ERROR_OK) {
            return err;
        }
    }

    ctx->update_cnt = 0;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_system_update_stub_impl_set_update(
    inf_system_update_stub_impl_ctx_t* ctx,
    const dom_models_update_info_t*    update_info
) {
    if (!ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    dom_models_error_t err = inf_system_update_stub_impl_validate_update_info(update_info);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    memcpy(&ctx->update_info, update_info, sizeof(dom_models_update_info_t));
    ctx->update_available = true;
    ctx->update_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t inf_system_update_stub_impl_validate_update_info(
    const dom_models_update_info_t* update_info
) {
    if (!update_info ||
        update_info->firmware_url[0] == '\0' ||
        update_info->firmware_size == 0 ||
        !sha256_checksum_valid(update_info->firmware_checksum)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

/* Helper Function Implementations */

static bool sha256_checksum_valid(const char* checksum) {
    if (!checksum || strlen(checksum) != 64) {
        return false;
    }

    for (size_t i = 0; i < 64; i++) {
        if (!isxdigit((unsigned char)checksum[i])) {
            return false;
        }
    }

    return true;
}
