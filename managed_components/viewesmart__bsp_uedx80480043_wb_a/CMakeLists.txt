idf_component_register(
    SRCS "src/bsp_uedx80480043_wb_a.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_ledc
        esp_driver_spi
        esp_driver_sdspi
        fatfs
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_lcd
        bt
    PRIV_REQUIRES
        esp_timer
        esp_psram
        spiffs
)
