idf_component_register(
    SRCS
        "main.c"
        "display.c"
        "rtc_pcf85063.c"
        "wifi_sync.c"
        "storage.c"
        "audio.c"
        "battery.c"
        "config_drive.c"
    INCLUDE_DIRS "."
    REQUIRES app lvgl esp_lcd driver esp_timer esp_wifi nvs_flash esp_netif spiffs esp_io_expander_tca9554 codec_board esp_adc wear_levelling fatfs espressif__esp_tinyusb
)

if(NOT "$ENV{TOMATO32_DISPLAY_PERF}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TOMATO32_DISPLAY_PERF=$ENV{TOMATO32_DISPLAY_PERF})
endif()
