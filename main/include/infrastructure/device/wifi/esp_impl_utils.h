#ifndef INFRASTRUCTURE_DEVICE_WIFI_ESP_IMPL_UTILS_H
#define INFRASTRUCTURE_DEVICE_WIFI_ESP_IMPL_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/models/error.h"
#include "esp_err.h"
#include "esp_netif_ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

dom_models_error_t inf_device_wifi_esp_impl_error_from_esp(esp_err_t err);
size_t             inf_device_wifi_esp_impl_bounded_strlen(const char* value, size_t max_len);
size_t             inf_device_wifi_esp_impl_bounded_byte_strlen(const uint8_t* value, size_t max_len);
void               inf_device_wifi_esp_impl_copy_cstr(char* dst, size_t dst_size, const char* src);
void               inf_device_wifi_esp_impl_copy_cstr_to_bytes(uint8_t* dst, size_t dst_size, const char* src);
void               inf_device_wifi_esp_impl_copy_bytes_to_cstr(char* dst, size_t dst_size, const uint8_t* src, size_t src_size);
void               inf_device_wifi_esp_impl_ip4_to_bytes(esp_ip4_addr_t addr, uint8_t out[4]);

#ifdef __cplusplus
}
#endif

#endif /* INFRASTRUCTURE_DEVICE_WIFI_ESP_IMPL_UTILS_H */
