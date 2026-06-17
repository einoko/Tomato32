set(WIFI_SYNC_SSID_ESC "${WIFI_SYNC_SSID}")
set(WIFI_SYNC_PASS_ESC "${WIFI_SYNC_PASS}")
set(WIFI_SYNC_TZ_ESC "${WIFI_SYNC_TZ}")

string(REPLACE "\\" "\\\\" WIFI_SYNC_SSID_ESC "${WIFI_SYNC_SSID_ESC}")
string(REPLACE "\"" "\\\"" WIFI_SYNC_SSID_ESC "${WIFI_SYNC_SSID_ESC}")
string(REPLACE "\\" "\\\\" WIFI_SYNC_PASS_ESC "${WIFI_SYNC_PASS_ESC}")
string(REPLACE "\"" "\\\"" WIFI_SYNC_PASS_ESC "${WIFI_SYNC_PASS_ESC}")
string(REPLACE "\\" "\\\\" WIFI_SYNC_TZ_ESC "${WIFI_SYNC_TZ_ESC}")
string(REPLACE "\"" "\\\"" WIFI_SYNC_TZ_ESC "${WIFI_SYNC_TZ_ESC}")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/wifi_sync_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/wifi_sync_config.h"
    @ONLY
)

idf_component_register(
    SRCS
        "main.c"
        "display.c"
        "rtc_pcf85063.c"
        "wifi_sync.c"
        "storage.c"
        "audio.c"
        "battery.c"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}/generated"
    REQUIRES app lvgl esp_lcd driver esp_timer esp_wifi nvs_flash esp_netif spiffs esp_io_expander_tca9554 codec_board esp_adc
)
