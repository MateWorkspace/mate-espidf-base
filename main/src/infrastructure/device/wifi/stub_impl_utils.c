#include "infrastructure/device/wifi/stub_impl_utils.h"

#include <string.h>

dom_models_error_t inf_device_wifi_stub_impl_bad_argument_error(void) {
    return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
}

size_t inf_device_wifi_stub_impl_bounded_strlen(const char* value, size_t max_len) {
    size_t len = 0;

    if (!value) {
        return 0;
    }

    while (len < max_len && value[len] != '\0') {
        len++;
    }

    return len;
}

void inf_device_wifi_stub_impl_copy_cstr(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) {
        return;
    }

    const char* value = src ? src : "";
    size_t      len   = inf_device_wifi_stub_impl_bounded_strlen(value, dst_size - 1);
    dst[len]          = '\0';

    if (len > 0) {
        memcpy(dst, value, len);
    }
}

void inf_device_wifi_stub_impl_fill_default_ipv4(uint8_t ipv4[4], uint8_t netmask[4], uint8_t gateway[4]) {
    if (ipv4) {
        ipv4[0] = 192;
        ipv4[1] = 168;
        ipv4[2] = 4;
        ipv4[3] = 2;
    }

    if (netmask) {
        netmask[0] = 255;
        netmask[1] = 255;
        netmask[2] = 255;
        netmask[3] = 0;
    }

    if (gateway) {
        gateway[0] = 192;
        gateway[1] = 168;
        gateway[2] = 4;
        gateway[3] = 1;
    }
}
