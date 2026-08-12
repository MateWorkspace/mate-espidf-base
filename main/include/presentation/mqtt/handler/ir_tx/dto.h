#ifndef PRESENTATION_MQTT_HANDLER_IR_TX_DTO_H
#define PRESENTATION_MQTT_HANDLER_IR_TX_DTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_MQTT_HANDLER_IR_TX_DTO_EXECUTION_ID_MAX_LEN 64
#define PRES_MQTT_HANDLER_IR_TX_DTO_RAW_DATA_MAX_LEN     512

typedef struct {
    char    execution_id[PRES_MQTT_HANDLER_IR_TX_DTO_EXECUTION_ID_MAX_LEN];
    bool    execution_id_set;
    int32_t raw_data[PRES_MQTT_HANDLER_IR_TX_DTO_RAW_DATA_MAX_LEN];
    size_t  raw_data_count;
    /* True if raw_data was truncated (array_size exceeds the max length
       above) or contained a non-numeric element - either case means
       raw_data no longer reflects the payload verbatim, which would
       silently corrupt mark/space parity if used as-is. Callers must
       treat this as a hard decode failure, not fall back to the
       partially-decoded raw_data. */
    bool raw_data_lossy;
} pres_mqtt_handler_ir_tx_dto_request_t;

/* Parses the /sub/<device_id>/ir/tx payload:
   {"execution_id": string, "raw_data": [int, ...]}. Returns
   DOMAIN_MODELS_ERROR_BAD_ARGUMENT if data is empty/not valid JSON; a
   missing execution_id or raw_data is reported via execution_id_set /
   raw_data_count == 0 instead of a decode error, matching the existing
   action dto's negative-case convention. */
dom_models_error_t pres_mqtt_handler_ir_tx_dto_decode(
    const char*                            data,
    int                                    data_len,
    pres_mqtt_handler_ir_tx_dto_request_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_IR_TX_DTO_H */
