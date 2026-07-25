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

target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE
        PROJECT_NAME="${APP_NAME}"
        PROJECT_VERSION="${PROJECT_VERSION}"
)
