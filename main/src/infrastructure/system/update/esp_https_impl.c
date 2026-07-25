#include "infrastructure/system/update/esp_https_impl.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "domain/contracts/system/update.h"
#include "domain/models/error.h"
#include "domain/models/update.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "infrastructure/system/update/esp_https_impl_utils.h"
#include "psa/crypto.h"

/* Helper Function Prototypes */

static void               abort_ota(inf_system_update_esp_https_impl_ctx_t* ctx);
static void               clear_ota_state(inf_system_update_esp_https_impl_ctx_t* ctx);
static void               normalize_cfg(inf_system_update_esp_https_impl_cfg_t* cfg);
static dom_models_error_t finalize_ota(inf_system_update_esp_https_impl_ctx_t* ctx);
static dom_models_error_t perform_update(
    inf_system_update_esp_https_impl_ctx_t* ctx,
    const dom_models_update_info_t*         update_info
);

/* Contract Function Prototypes */

static dom_models_error_t update_impl(
    dom_contracts_system_update_t*  self,
    const dom_models_update_info_t* update_info
);
static dom_models_error_t validate_impl(
    dom_contracts_system_update_t* self
);
static dom_models_error_t rollback_impl(
    dom_contracts_system_update_t* self
);

/* Constructor and Destructor */

dom_contracts_system_update_t* inf_system_update_esp_https_impl_new(
    const inf_system_update_esp_https_impl_cfg_t* cfg
) {
    inf_system_update_esp_https_impl_ctx_t* ctx = (inf_system_update_esp_https_impl_ctx_t*)calloc(1, sizeof(inf_system_update_esp_https_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    inf_system_update_esp_https_impl_cfg_t default_cfg = INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_CFG_DEFAULT();
    memcpy(&ctx->cfg, cfg ? cfg : &default_cfg, sizeof(inf_system_update_esp_https_impl_cfg_t));
    normalize_cfg(&ctx->cfg);

    dom_contracts_system_update_t* self = dom_contracts_system_update_new(ctx);
    if (!self) {
        free(ctx);
        return NULL;
    }

    self->update   = update_impl;
    self->validate = validate_impl;
    self->rollback = rollback_impl;

    return self;
}

void inf_system_update_esp_https_impl_delete(dom_contracts_system_update_t* self) {
    if (!self) {
        return;
    }

    inf_system_update_esp_https_impl_ctx_t* ctx = self->ctx;
    if (ctx) {
        abort_ota(ctx);
        free(ctx);
    }

    dom_contracts_system_update_delete(self);
}

/* Contract Function Implementations */

static dom_models_error_t update_impl(
    dom_contracts_system_update_t*  self,
    const dom_models_update_info_t* update_info
) {
    if (!self || !self->ctx || !inf_system_update_esp_https_impl_update_info_valid(update_info)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    inf_system_update_esp_https_impl_ctx_t* ctx = self->ctx;
    if (ctx->ota_started) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    return perform_update(ctx, update_info);
}

static dom_models_error_t validate_impl(
    dom_contracts_system_update_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    const esp_partition_t* running_partition = esp_ota_get_running_partition();
    if (!running_partition) {
        return DOMAIN_MODELS_ERROR_NOT_FOUND;
    }

    esp_ota_img_states_t ota_state;
    esp_err_t            err = esp_ota_get_state_partition(running_partition, &ota_state);
    if (err != ESP_OK) {
        return inf_system_update_esp_https_impl_error_from_esp(err);
    }

    if (ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
        return DOMAIN_MODELS_ERROR_OK;
    }

    err = esp_ota_mark_app_valid_cancel_rollback();
    return inf_system_update_esp_https_impl_error_from_esp(err);
#else
    return DOMAIN_MODELS_ERROR_OK;
#endif
}

static dom_models_error_t rollback_impl(
    dom_contracts_system_update_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    return inf_system_update_esp_https_impl_error_from_esp(err);
#else
    return DOMAIN_MODELS_ERROR_BAD_STATE;
#endif
}

/* Helper Function Implementations */

static void abort_ota(inf_system_update_esp_https_impl_ctx_t* ctx) {
    if (!ctx || !ctx->ota_started) {
        return;
    }

    (void)esp_ota_abort(ctx->update_handle);
    clear_ota_state(ctx);
}

static void clear_ota_state(inf_system_update_esp_https_impl_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    ctx->update_partition = NULL;
    ctx->update_handle    = 0;
    ctx->ota_started      = false;
}

static void normalize_cfg(inf_system_update_esp_https_impl_cfg_t* cfg) {
    if (!cfg) {
        return;
    }

    if (cfg->http_timeout_ms <= 0) {
        cfg->http_timeout_ms = INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_DEFAULT_HTTP_TIMEOUT_MS;
    }
    if (cfg->http_read_buffer_size <= 0) {
        cfg->http_read_buffer_size = INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_DEFAULT_HTTP_READ_BUFFER_SIZE;
    }
    if (cfg->max_empty_read_count <= 0) {
        cfg->max_empty_read_count = INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_DEFAULT_MAX_EMPTY_READ_COUNT;
    }
}

static dom_models_error_t finalize_ota(inf_system_update_esp_https_impl_ctx_t* ctx) {
    if (!ctx || !ctx->ota_started || !ctx->update_partition) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    esp_err_t err = esp_ota_end(ctx->update_handle);
    if (err != ESP_OK) {
        clear_ota_state(ctx);
        return inf_system_update_esp_https_impl_error_from_esp(err);
    }

    err = esp_ota_set_boot_partition(ctx->update_partition);
    clear_ota_state(ctx);

    return inf_system_update_esp_https_impl_error_from_esp(err);
}

static dom_models_error_t perform_update(
    inf_system_update_esp_https_impl_ctx_t* ctx,
    const dom_models_update_info_t*         update_info
) {
    char expected_checksum[INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN + 1];
    if (!inf_system_update_esp_https_impl_normalize_sha256_hex(update_info->firmware_checksum, expected_checksum)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        return DOMAIN_MODELS_ERROR_NOT_FOUND;
    }

    if (update_info->firmware_size > update_partition->size) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    uint8_t* read_buffer = (uint8_t*)calloc((size_t)ctx->cfg.http_read_buffer_size, sizeof(uint8_t));
    if (!read_buffer) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    esp_http_client_config_t http_cfg = {
        .url                         = update_info->firmware_url,
        .cert_pem                    = ctx->cfg.cert_pem,
        .timeout_ms                  = ctx->cfg.http_timeout_ms,
        .buffer_size                 = ctx->cfg.http_read_buffer_size,
        .keep_alive_enable           = ctx->cfg.keep_alive_enable,
        .skip_cert_common_name_check = ctx->cfg.skip_cert_common_name_check,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client) {
        free(read_buffer);
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    dom_models_error_t   result        = DOMAIN_MODELS_ERROR_FAILURE;
    psa_hash_operation_t hash_op       = PSA_HASH_OPERATION_INIT;
    bool                 client_opened = false;
    bool                 hash_started  = false;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        result = inf_system_update_esp_https_impl_error_from_esp(err);
        goto cleanup;
    }
    client_opened = true;

    int64_t content_length = esp_http_client_fetch_headers(client);
    int     status_code    = esp_http_client_get_status_code(client);
    if (status_code < 200 || status_code >= 300) {
        goto cleanup;
    }

    if (content_length >= 0 && (uint64_t)content_length != update_info->firmware_size) {
        result = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        goto cleanup;
    }

    err = esp_ota_begin(update_partition, update_info->firmware_size, &ctx->update_handle);
    if (err != ESP_OK) {
        result = inf_system_update_esp_https_impl_error_from_esp(err);
        goto cleanup;
    }
    ctx->update_partition = update_partition;
    ctx->ota_started      = true;

    psa_status_t psa_status = psa_crypto_init();
    if (psa_status != PSA_SUCCESS) {
        goto cleanup;
    }

    psa_status = psa_hash_setup(&hash_op, PSA_ALG_SHA_256);
    if (psa_status != PSA_SUCCESS) {
        goto cleanup;
    }
    hash_started = true;

    size_t total_read       = 0;
    int    empty_read_count = 0;
    while (true) {
        errno         = 0;
        int data_read = esp_http_client_read(client, (char*)read_buffer, ctx->cfg.http_read_buffer_size);
        if (data_read < 0) {
            goto cleanup;
        }

        if (data_read == 0) {
            if (esp_http_client_is_complete_data_received(client)) {
                break;
            }
            if (errno == ECONNRESET || errno == ENOTCONN) {
                goto cleanup;
            }
            empty_read_count++;
            if (empty_read_count > ctx->cfg.max_empty_read_count) {
                result = DOMAIN_MODELS_ERROR_TIMEOUT;
                goto cleanup;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        empty_read_count = 0;

        size_t chunk_size = (size_t)data_read;
        if (total_read > update_info->firmware_size || chunk_size > update_info->firmware_size - total_read) {
            result = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
            goto cleanup;
        }

        psa_status = psa_hash_update(&hash_op, read_buffer, chunk_size);
        if (psa_status != PSA_SUCCESS) {
            goto cleanup;
        }

        err = esp_ota_write(ctx->update_handle, read_buffer, chunk_size);
        if (err != ESP_OK) {
            result = inf_system_update_esp_https_impl_error_from_esp(err);
            goto cleanup;
        }

        total_read += chunk_size;
    }

    if (!esp_http_client_is_complete_data_received(client)) {
        goto cleanup;
    }

    if (total_read != update_info->firmware_size) {
        result = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        goto cleanup;
    }

    uint8_t digest[32];
    char    actual_checksum[INF_SYSTEM_UPDATE_ESP_HTTPS_IMPL_SHA256_HEX_LEN + 1];
    size_t  digest_len = 0;
    psa_status         = psa_hash_finish(&hash_op, digest, sizeof(digest), &digest_len);
    if (psa_status != PSA_SUCCESS || digest_len != sizeof(digest)) {
        goto cleanup;
    }
    hash_started = false;

    inf_system_update_esp_https_impl_sha256_to_hex(digest, actual_checksum);
    if (strcmp(actual_checksum, expected_checksum) != 0) {
        result = DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
        goto cleanup;
    }

    result = finalize_ota(ctx);

cleanup:
    if (result != DOMAIN_MODELS_ERROR_OK) {
        abort_ota(ctx);
    }
    if (hash_started) {
        (void)psa_hash_abort(&hash_op);
    }
    if (client_opened) {
        esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);
    free(read_buffer);

    return result;
}
