file(GLOB_RECURSE srcsc "src/*.c")

idf_component_register(
    SRCS 
        ${srcsc}
    INCLUDE_DIRS 
        "include"
    REQUIRES
        driver
        esp_driver_gpio
        esp_driver_rmt
        esp_timer
        freertos
)
