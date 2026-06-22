#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

typedef int (*app_battery_percent_provider_t)(void);
typedef bool (*app_battery_charging_provider_t)(void);
typedef void (*app_brightness_provider_t)(uint8_t percent);
typedef void (*app_bell_volume_provider_t)(uint8_t percent);
typedef void (*app_power_off_provider_t)(void);

typedef enum
{
  APP_SCREEN_TIMER,
  APP_SCREEN_SETTINGS,
  APP_SCREEN_STATS,
  APP_SCREEN_DEBUG,
} app_screen_t;

typedef const char *(*app_debug_str_provider_t)(void);
typedef uint32_t (*app_free_heap_provider_t)(void);

typedef void (*app_ntp_sync_provider_t)(void);

void app_init(lv_display_t *display);
void app_show_timer_screen(void);
void app_show_settings_screen(void);
void app_show_stats_screen(void);
void app_show_debug_screen(void);
app_screen_t app_get_active_screen(void);
void app_set_battery_percent_provider(app_battery_percent_provider_t provider);
void app_set_battery_charging_provider(
    app_battery_charging_provider_t provider);
void app_set_brightness_provider(app_brightness_provider_t provider);
void app_set_bell_volume_provider(app_bell_volume_provider_t provider);
void app_set_power_off_provider(app_power_off_provider_t provider);
int app_get_battery_percent(void);
bool app_is_battery_charging(void);
void app_notify_user_activity(void);
void app_timer_toggle(void);
void app_invalidate_pause_state(void);
void app_set_wifi_ssid_provider(app_debug_str_provider_t provider);
void app_set_wifi_pass_provider(app_debug_str_provider_t provider);
void app_set_ip_addr_provider(app_debug_str_provider_t provider);
void app_set_free_heap_provider(app_free_heap_provider_t provider);
void app_set_ntp_sync_provider(app_ntp_sync_provider_t provider);
void app_request_ntp_sync(void);
const char *app_get_wifi_ssid(void);
const char *app_get_wifi_pass(void);
const char *app_get_ip_addr(void);
uint32_t app_get_free_heap(void);

#endif /* APP_H */