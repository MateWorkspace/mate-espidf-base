#include "presentation/ble/gatt/registry.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "host/ble_gatt.h"

#define PRES_BLE_GATT_REGISTRY_MAX_SERVICES 8

struct pres_ble_gatt_registry_t {
    const struct ble_gatt_svc_def* services[PRES_BLE_GATT_REGISTRY_MAX_SERVICES];
    unsigned int                   count;
    bool                           registered;
};

/* ble_gatts_add_svcs() retains a pointer to this array for the lifetime of
   the BLE host, same lifetime requirement as each feature's own svc_def -
   so this combined array must be static/global too, not stack or per-ctx
   heap memory that could be freed while NimBLE still points at it. There is
   only ever one registry in this app, so a single file-scope array (rather
   than something allocated per pres_ble_gatt_registry_t instance) is both
   correct and simplest. */
static struct ble_gatt_svc_def combined_defs[PRES_BLE_GATT_REGISTRY_MAX_SERVICES + 1];

pres_ble_gatt_registry_t* pres_ble_gatt_registry_new(void) {
    return (pres_ble_gatt_registry_t*)calloc(1, sizeof(pres_ble_gatt_registry_t));
}

void pres_ble_gatt_registry_delete(pres_ble_gatt_registry_t* self) {
    free(self);
}

dom_models_error_t pres_ble_gatt_registry_add_service(
    pres_ble_gatt_registry_t*      self,
    const struct ble_gatt_svc_def* svc
) {
    if (!self || !svc) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->registered) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }
    if (self->count >= PRES_BLE_GATT_REGISTRY_MAX_SERVICES) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    self->services[self->count] = svc;
    self->count += 1;

    return DOMAIN_MODELS_ERROR_OK;
}

dom_models_error_t pres_ble_gatt_registry_register_all(pres_ble_gatt_registry_t* self) {
    if (!self) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
    if (self->registered) {
        return DOMAIN_MODELS_ERROR_BAD_STATE;
    }

    memset(combined_defs, 0, sizeof(combined_defs));
    for (unsigned int i = 0; i < self->count; i++) {
        combined_defs[i] = *self->services[i];
    }
    /* combined_defs[self->count] stays zeroed - the required terminator. */

    int rc = ble_gatts_count_cfg(combined_defs);
    if (rc != 0) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    rc = ble_gatts_add_svcs(combined_defs);
    if (rc != 0) {
        return DOMAIN_MODELS_ERROR_FAILURE;
    }

    self->registered = true;

    return DOMAIN_MODELS_ERROR_OK;
}
