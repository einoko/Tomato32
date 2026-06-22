#include "app.h"
#include "debug_screen.h"
#include "pomodoro.h"
#include "settings_screen.h"
#include "stats_screen.h"
#include "theme.h"
#include "timer_screen.h"

static lv_obj_t *timer_scr;
static lv_obj_t *settings_scr;
static lv_obj_t *stats_scr;
static lv_obj_t *debug_scr;
static lv_timer_t *tick_timer;
static uint32_t last_tick_ms = 0;
static uint32_t pause_elapsed_in_second = 0;
static bool pause_elapsed_valid = false;
static bool restore_period_needed = false;
static app_battery_percent_provider_t battery_provider;
static app_battery_charging_provider_t battery_charging_provider;
static app_brightness_provider_t brightness_provider;
static app_bell_volume_provider_t bell_volume_provider;
static app_power_off_provider_t power_off_provider;
static app_debug_str_provider_t wifi_ssid_provider;
static app_debug_str_provider_t wifi_pass_provider;
static app_debug_str_provider_t ip_addr_provider;
static app_free_heap_provider_t free_heap_provider;
static app_ntp_sync_provider_t ntp_sync_provider;
static app_screen_t active_screen = APP_SCREEN_TIMER;
static uint8_t s_last_brightness_percent = 0;
static bool s_last_brightness_valid = false;

#define SMART_DIM_TIMEOUT_MS (60 * 1000U)

static void apply_screen_brightness(uint8_t brightness_percent) {
  if (brightness_provider) {
    if (s_last_brightness_valid &&
        s_last_brightness_percent == brightness_percent) {
      return;
    }
    brightness_provider(brightness_percent);
    s_last_brightness_percent = brightness_percent;
    s_last_brightness_valid = true;
  }
}

static void apply_bell_volume(uint8_t volume_percent) {
  if (bell_volume_provider) {
    bell_volume_provider(volume_percent);
  }
}

static void update_brightness_policy(bool phase_just_completed) {
  uint8_t normal = pomodoro_get_default_brightness();
  bool running = pomodoro_is_running();

  if (phase_just_completed) {
    lv_display_trigger_activity(NULL);
    apply_screen_brightness(normal);
    return;
  }

  bool smart_dim = pomodoro_get_smart_dim();
  bool smart_sleep = pomodoro_get_power_nap_mode();

  if (!running || (!smart_dim && !smart_sleep)) {
    apply_screen_brightness(normal);
    return;
  }

  uint32_t inactive_ms = lv_display_get_inactive_time(NULL);

  if (inactive_ms >= SMART_DIM_TIMEOUT_MS) {
    if (smart_sleep) {
      apply_screen_brightness(0);
    } else {
      apply_screen_brightness(pomodoro_get_smart_dim_brightness());
    }
  } else {
    apply_screen_brightness(normal);
  }
}

static void tick_cb(lv_timer_t *timer) {
  (void)timer;

  last_tick_ms = lv_tick_get();
  if (restore_period_needed) {
    lv_timer_set_period(tick_timer, 1000);
    restore_period_needed = false;
  }

  uint32_t before_remaining = pomodoro_get_remaining();
  bool was_running = pomodoro_is_running();

  pomodoro_tick();

  bool phase_just_completed = false;
  if (was_running) {
    uint32_t after_remaining = pomodoro_get_remaining();
    if (before_remaining > 0 && after_remaining > before_remaining) {
      phase_just_completed = true;
    }
  }

  update_brightness_policy(phase_just_completed);
  apply_bell_volume(pomodoro_get_bell_volume());

  if (active_screen == APP_SCREEN_TIMER) {
    timer_screen_update();
  } else if (active_screen == APP_SCREEN_SETTINGS) {
    settings_screen_update();
  } else if (active_screen == APP_SCREEN_STATS) {
    stats_screen_update();
  } else if (active_screen == APP_SCREEN_DEBUG) {
    debug_screen_update();
  }
}

void app_init(lv_display_t *display) {
  theme_init(display);
  pomodoro_init();

  timer_scr = timer_screen_create();
  settings_scr = settings_screen_create();
  stats_scr = stats_screen_create();
  debug_scr = debug_screen_create();

  timer_screen_update();
  lv_scr_load(timer_scr);
  active_screen = APP_SCREEN_TIMER;

  tick_timer = lv_timer_create(tick_cb, 1000, NULL);
  (void)display;
  apply_screen_brightness(pomodoro_get_default_brightness());
  apply_bell_volume(pomodoro_get_bell_volume());
}

void app_show_timer_screen(void) {
  lv_scr_load(timer_scr);
  active_screen = APP_SCREEN_TIMER;
  timer_screen_update();
}

void app_show_settings_screen(void) {
  settings_screen_update();
  lv_scr_load(settings_scr);
  active_screen = APP_SCREEN_SETTINGS;
}

void app_show_stats_screen(void) {
  stats_screen_update();
  lv_scr_load(stats_scr);
  active_screen = APP_SCREEN_STATS;
}

void app_show_debug_screen(void) {
  debug_screen_update();
  lv_scr_load(debug_scr);
  active_screen = APP_SCREEN_DEBUG;
}

app_screen_t app_get_active_screen(void) { return active_screen; }

void app_set_battery_percent_provider(app_battery_percent_provider_t provider) {
  battery_provider = provider;
}

void app_set_battery_charging_provider(
    app_battery_charging_provider_t provider) {
  battery_charging_provider = provider;
}

void app_set_brightness_provider(app_brightness_provider_t provider) {
  brightness_provider = provider;
  s_last_brightness_valid = false;
  apply_screen_brightness(pomodoro_get_default_brightness());
}

void app_set_bell_volume_provider(app_bell_volume_provider_t provider) {
  bell_volume_provider = provider;
  apply_bell_volume(pomodoro_get_bell_volume());
}

void app_set_power_off_provider(app_power_off_provider_t provider) {
  power_off_provider = provider;
}

void app_set_wifi_ssid_provider(app_debug_str_provider_t provider) {
  wifi_ssid_provider = provider;
}

void app_set_wifi_pass_provider(app_debug_str_provider_t provider) {
  wifi_pass_provider = provider;
}

void app_set_ip_addr_provider(app_debug_str_provider_t provider) {
  ip_addr_provider = provider;
}

void app_set_free_heap_provider(app_free_heap_provider_t provider) {
  free_heap_provider = provider;
}

void app_set_ntp_sync_provider(app_ntp_sync_provider_t provider) {
  ntp_sync_provider = provider;
}

void app_request_ntp_sync(void) {
  if (ntp_sync_provider) {
    ntp_sync_provider();
  }
}

const char *app_get_wifi_ssid(void) {
  return wifi_ssid_provider ? wifi_ssid_provider() : "";
}

const char *app_get_wifi_pass(void) {
  return wifi_pass_provider ? wifi_pass_provider() : "";
}

const char *app_get_ip_addr(void) {
  return ip_addr_provider ? ip_addr_provider() : "";
}

uint32_t app_get_free_heap(void) {
  return free_heap_provider ? free_heap_provider() : 0;
}

int app_get_battery_percent(void) {
  if (battery_provider) {
    return battery_provider();
  }

  return -1;
}

bool app_is_battery_charging(void) {
  if (battery_charging_provider) {
    return battery_charging_provider();
  }

  return false;
}

void app_timer_toggle(void) {
  bool was_running = pomodoro_is_running();
  pomodoro_start_pause();
  bool is_running = pomodoro_is_running();

  if (!was_running && is_running) {
    if (pause_elapsed_valid) {
      /* Resume: fire the first tick after the remainder of the interrupted
       * second, then restore the normal 1000 ms period. */
      uint32_t remaining_ms = 1000 - pause_elapsed_in_second;
      if (remaining_ms == 0 || remaining_ms > 1000)
        remaining_ms = 1000;
      lv_timer_set_period(tick_timer, remaining_ms);
      lv_timer_reset(tick_timer);
      restore_period_needed = true;
      pause_elapsed_valid = false;
    } else {
      /* Fresh start: first tick in exactly 1000 ms. */
      lv_timer_reset(tick_timer);
    }
  } else if (was_running && !is_running) {
    /* Pause: record how far into the current second we are. */
    uint32_t elapsed = lv_tick_get() - last_tick_ms;
    pause_elapsed_in_second = (elapsed < 1000) ? elapsed : 0;
    pause_elapsed_valid = true;
  }
}

void app_invalidate_pause_state(void) { pause_elapsed_valid = false; }

void app_notify_user_activity(void) {
  lv_display_trigger_activity(NULL);
  update_brightness_policy(false);
}