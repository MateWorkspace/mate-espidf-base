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
        infrared
        # idf components
        cjson
        littlefs
        esp_websocket_client
)

idf_build_get_property(mate_project_ver PROJECT_VER)
idf_build_get_property(mate_node_class_name MATE_NODE_CLASS_NAME)

target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE
        # Used by cmp_main_utils_* (MQTT client ID, BLE device name).
        PROJECT_NAME="${CMAKE_PROJECT_NAME}"
        PROJECT_VER="${mate_project_ver}"
        NODE_CLASS_NAME="${mate_node_class_name}"
)
