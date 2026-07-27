#include "presentation/ble/gatt/util.h"

#include "host/ble_att.h"
#include "host/ble_hs.h"

int pres_ble_gatt_util_disabled_access_callback(
    uint16_t                     conn_handle,
    uint16_t                     attr_handle,
    struct ble_gatt_access_ctxt* ctxt,
    void*                        arg
) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (!ctxt) {
        return BLE_ATT_ERR_INVALID_HANDLE;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return 0;
    }
    return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
}

int pres_ble_gatt_util_read_write_payload(
    struct ble_gatt_access_ctxt* ctxt,
    char*                        buf,
    size_t                       buf_cap,
    size_t*                      out_len
) {
    if (!ctxt || !buf || buf_cap == 0) {
        return BLE_ATT_ERR_INVALID_HANDLE;
    }

    uint16_t om_len   = OS_MBUF_PKTLEN(ctxt->om);
    size_t   copy_len = (size_t)om_len < (buf_cap - 1) ? (size_t)om_len : (buf_cap - 1);

    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, copy_len, NULL);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    buf[copy_len] = '\0';
    if (out_len) {
        *out_len = copy_len;
    }

    return 0;
}

int pres_ble_gatt_util_write_read_response(
    struct ble_gatt_access_ctxt* ctxt,
    const char*                  data,
    size_t                       len
) {
    if (!ctxt) {
        return BLE_ATT_ERR_INVALID_HANDLE;
    }

    int rc = os_mbuf_append(ctxt->om, data, (uint16_t)len);
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}
