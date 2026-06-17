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

typedef enum {
  APP_SCREEN_TIMER,
  APP_SCREEN_SETTINGS,
  APP_SCREEN_STATS,
} app_screen_t;

void app_init(lv_display_t *display);
void app_show_timer_screen(void);
void app_show_settings_screen(void);
void app_show_stats_screen(void);
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

#endif /* APP_H */