#ifndef PRESENTATION_BLE_GATT_UTIL_H
#define PRESENTATION_BLE_GATT_UTIL_H

#include <stddef.h>
#include <stdint.h>

#include "host/ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A GATT access callback usable for a characteristic that has to stay
   registered (NimBLE never lets you unregister one - see gatt/registry.h's
   comment) but whose owning feature has been torn down. Reads return an
   empty value instead of erroring, writes/other ops are rejected. Shared
   so every handler's teardown path does the same thing instead of each
   defining its own near-identical callback. */
int pres_ble_gatt_util_disabled_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
);

/* Copies an incoming write's payload into a caller-provided fixed buffer,
   truncating (not erroring) if it doesn't fit, and NUL-terminating so it's
   always safe to treat as a C string (every write payload this app defines
   is either text/JSON or a small fixed binary value read separately). */
int pres_ble_gatt_util_read_write_payload(
    struct ble_gatt_access_ctxt* ctxt,
    char*                        buf,
    size_t                       buf_cap,
    size_t*                      out_len
);

/* Appends a buffer to a read response's mbuf, returning a NimBLE ATT status
   code (0 on success) - the single line every read access callback needs. */
int pres_ble_gatt_util_write_read_response(
    struct ble_gatt_access_ctxt* ctxt,
    const char*                  data,
    size_t                       len
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_BLE_GATT_UTIL_H */
