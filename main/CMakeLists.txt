file(
    GLOB_RECURSE srcs
    "src/*.c"
    "src/*.cpp"
)

idf_component_register(
    SRCS 
        "main.c"
        ${srcs}
    INCLUDE_DIRS
        "include"
    PRIV_REQUIRES
        # built-in
        freertos
        spi_flash
        nvs_flash
        driver
        bt
        esp_timer
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_spi
        esp_driver_sdspi
        sdmmc
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        esp_http_client
        esp_https_ota
        fatfs
        app_update
        mbedtls
        mqtt
        # components
        # idf components
        cjson
        littlefs
        esp_websocket_client
)

# ../version.txt (read by ESP-IDF's own project.cmake) only feeds the
# runtime esp_app_desc_t struct (what the boot log's "App version:" line
# reads) - it does NOT, by itself, define a PROJECT_VER *compile-time macro*
# for component sources. infrastructure/system/info/esp_impl_types.h's
# `#ifndef PROJECT_VER` fallback ("unknown") needs the latter, and that's
# what silently broke the device's own MQTT registration payload
# (firmware_name ended up "<name>_unknown", matching no real firmware row,
# even though the boot banner's version looked completely correct - these
# are two independent mechanisms). idf_build_get_property() below reads
# back whatever ESP-IDF itself already resolved PROJECT_VER to (version.txt
# > CONFIG_APP_PROJECT_VER_FROM_CONFIG > git describe > default "1", in that
# priority order), so this stays correct regardless of which source wins.
idf_build_get_property(mate_project_ver PROJECT_VER)

target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE
        # Used by cmp_main_utils_* (MQTT client ID, BLE device name).
        PROJECT_NAME="${CMAKE_PROJECT_NAME}"
        PROJECT_VER="${mate_project_ver}"
)
