#ifndef INFRASTRUCTURE_DEVICE_WIFI_STUB_IMPL_UTILS_H
#define INFRASTRUCTURE_DEVICE_WIFI_STUB_IMPL_UTILS_H

#include <stddef.h>
#include <stdint.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_device_wifi_stub_impl_bad_argument_error(void);
size_t             inf_device_wifi_stub_impl_bounded_strlen(const char* value, size_t max_len);
void               inf_device_wifi_stub_impl_copy_cstr(char* dst, size_t dst_size, const char* src);
void               inf_device_wifi_stub_impl_fill_default_ipv4(uint8_t ipv4[4], uint8_t netmask[4], uint8_t gateway[4]);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_WIFI_STUB_IMPL_UTILS_H */
