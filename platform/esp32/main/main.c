#include <inttypes.h>
#include <sys/time.h>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app/app.h"
#include "audio.h"
#include "battery.h"
#include "config_drive.h"
#include "display.h"
#include "rtc_pcf85063.h"
#include "storage.h"
#include "wifi_sync.h"

static const char *TAG = "main";

#define SETTINGS_BUTTON_DEBOUNCE_MS 50
#define SETTINGS_BUTTON_LONG_PRESS_MS 1200
#define SETTINGS_BUTTON_GPIO GPIO_NUM_16
#define SETTINGS_BUTTON_ACTIVE_LEVEL 0

#define BOOT_BUTTON_DEBOUNCE_MS 50
#define BOOT_BUTTON_LONG_PRESS_MS 1200
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define BOOT_BUTTON_ACTIVE_LEVEL 0

#define WIFI_SYNC_ATTEMPT_INTERVAL_MS (24 * 60 * 60 * 1000)
#define WIFI_RESYNC_TIMEOUT_MS (30 * 1000)

typedef struct {
  gpio_num_t gpio;
  int active_level;
  uint32_t debounce_ms;
  uint32_t long_press_ms;
  int stable_level;
  TickType_t last_change_tick;
  TickType_t press_start_tick;
  bool suppress_next_release;
} button_state_t;

static QueueHandle_t s_button_event_queue;
static button_state_t s_settings_button = {
    .gpio = SETTINGS_BUTTON_GPIO,
    .active_level = SETTINGS_BUTTON_ACTIVE_LEVEL,
    .debounce_ms = SETTINGS_BUTTON_DEBOUNCE_MS,
    .long_press_ms = SETTINGS_BUTTON_LONG_PRESS_MS,
};
static button_state_t s_boot_button = {
    .gpio = BOOT_BUTTON_GPIO,
    .active_level = BOOT_BUTTON_ACTIVE_LEVEL,
    .debounce_ms = BOOT_BUTTON_DEBOUNCE_MS,
    .long_press_ms = BOOT_BUTTON_LONG_PRESS_MS,
};

static void IRAM_ATTR button_isr_handler(void *arg) {
  gpio_num_t gpio = (gpio_num_t)(uintptr_t)arg;
  BaseType_t high_task_woken = pdFALSE;
  xQueueSendFromISR(s_button_event_queue, &gpio, &high_task_woken);
  if (high_task_woken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

static button_state_t *button_state_from_gpio(gpio_num_t gpio) {
  if (gpio == s_settings_button.gpio) {
    return &s_settings_button;
  }
  if (gpio == s_boot_button.gpio) {
    return &s_boot_button;
  }
  return NULL;
}

static bool settings_button_gpio_init(void) {
  gpio_config_t cfg = {
      .pin_bit_mask = (1ULL << SETTINGS_BUTTON_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE,
  };

  esp_err_t err = gpio_config(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure settings GPIO %d (%d)",
             SETTINGS_BUTTON_GPIO, err);
    return false;
  }

  ESP_LOGI(TAG, "Settings button mapped to GPIO %d (active-%s)",
           SETTINGS_BUTTON_GPIO,
           SETTINGS_BUTTON_ACTIVE_LEVEL == 0 ? "low" : "high");
  return true;
}

static bool boot_button_gpio_init(void) {
  gpio_config_t cfg = {
      .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE,
  };

  esp_err_t err = gpio_config(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure BOOT GPIO %d (%d)", BOOT_BUTTON_GPIO,
             err);
    return false;
  }

  ESP_LOGI(TAG, "BOOT button mapped to GPIO %d (active-%s)", BOOT_BUTTON_GPIO,
           BOOT_BUTTON_ACTIVE_LEVEL == 0 ? "low" : "high");
  return true;
}

static void handle_settings_release(TickType_t now, button_state_t *button) {
  uint32_t press_ms =
      (uint32_t)((now - button->press_start_tick) * portTICK_PERIOD_MS);
  if (press_ms >= button->long_press_ms) {
    ESP_LOGI(TAG, "Physical key long press (%" PRIu32 "ms) -> power off",
             press_ms);
    if (display_power_off()) {
      for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }
  } else {
    ESP_LOGI(TAG, "Physical key short press (%" PRIu32 "ms) ignored", press_ms);
  }
}

static void handle_boot_release(TickType_t now, button_state_t *button) {
  uint32_t press_ms =
      (uint32_t)((now - button->press_start_tick) * portTICK_PERIOD_MS);

  if (display_lock(100)) {
    if (press_ms >= button->long_press_ms) {
      ESP_LOGI(TAG, "BOOT long press (%" PRIu32 "ms) -> stats screen",
               press_ms);
      app_show_stats_screen();
    } else {
      app_screen_t active_screen = app_get_active_screen();
      if (active_screen == APP_SCREEN_STATS ||
          active_screen == APP_SCREEN_SETTINGS) {
        ESP_LOGI(TAG, "BOOT short press (%" PRIu32 "ms) -> timer screen",
                 press_ms);
        app_show_timer_screen();
      } else {
        ESP_LOGI(TAG, "BOOT short press (%" PRIu32 "ms) -> settings screen",
                 press_ms);
        app_show_settings_screen();
      }
    }

    display_unlock();
  }
}

static void button_event_task(void *arg) {
  (void)arg;

  gpio_num_t gpio;
  for (;;) {
    if (xQueueReceive(s_button_event_queue, &gpio, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    button_state_t *button = button_state_from_gpio(gpio);
    if (!button) {
      continue;
    }

    TickType_t now = xTaskGetTickCount();
    if ((now - button->last_change_tick) < pdMS_TO_TICKS(button->debounce_ms)) {
      continue;
    }

    int level = gpio_get_level(button->gpio);
    if (level == button->stable_level) {
      continue;
    }

    button->last_change_tick = now;
    int previous_stable = button->stable_level;
    button->stable_level = level;

    bool pressed_edge = (button->stable_level == button->active_level) &&
                        (previous_stable != button->active_level);
    bool released_edge = (button->stable_level != button->active_level) &&
                         (previous_stable == button->active_level);

    if (pressed_edge) {
      button->press_start_tick = now;
      if (button == &s_boot_button && display_lock(100)) {
        app_notify_user_activity();
        display_unlock();
      }
      continue;
    }

    if (!released_edge) {
      continue;
    }

    if (button->suppress_next_release) {
      button->suppress_next_release = false;
      ESP_LOGI(TAG, "GPIO %d startup release ignored", button->gpio);
      continue;
    }

    if (button == &s_settings_button) {
      handle_settings_release(now, button);
    } else if (button == &s_boot_button) {
      handle_boot_release(now, button);
    }
  }
}

static bool button_input_init(void) {
  if (!settings_button_gpio_init() || !boot_button_gpio_init()) {
    return false;
  }

  s_settings_button.stable_level = gpio_get_level(s_settings_button.gpio);
  s_boot_button.stable_level = gpio_get_level(s_boot_button.gpio);
  s_settings_button.last_change_tick = xTaskGetTickCount();
  s_boot_button.last_change_tick = s_settings_button.last_change_tick;
  s_settings_button.press_start_tick = 0;
  s_boot_button.press_start_tick = 0;
  s_settings_button.suppress_next_release =
      (s_settings_button.stable_level == s_settings_button.active_level);
  s_boot_button.suppress_next_release =
      (s_boot_button.stable_level == s_boot_button.active_level);

  if (s_settings_button.suppress_next_release) {
    ESP_LOGI(TAG, "GPIO %d starts pressed; first release will be ignored",
             s_settings_button.gpio);
  }
  if (s_boot_button.suppress_next_release) {
    ESP_LOGI(TAG, "GPIO %d starts pressed; first release will be ignored",
             s_boot_button.gpio);
  }

  s_button_event_queue = xQueueCreate(16, sizeof(gpio_num_t));
  if (!s_button_event_queue) {
    ESP_LOGE(TAG, "Failed to create button event queue");
    return false;
  }

  esp_err_t err = gpio_install_isr_service(0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to install GPIO ISR service (%d)", err);
    return false;
  }

  err = gpio_isr_handler_add(s_settings_button.gpio, button_isr_handler,
                             (void *)(uintptr_t)s_settings_button.gpio);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add settings button ISR (%d)", err);
    return false;
  }

  err = gpio_isr_handler_add(s_boot_button.gpio, button_isr_handler,
                             (void *)(uintptr_t)s_boot_button.gpio);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add boot button ISR (%d)", err);
    return false;
  }

  return true;
}

static void wifi_resync_task(void *arg) {
  (void)arg;

  for (;;) {
    bool synced = wifi_sync_request_sync(WIFI_RESYNC_TIMEOUT_MS);
    if (!synced) {
      ESP_LOGW(TAG, "NTP sync attempt failed; retrying in 24h");
    } else {
      ESP_LOGI(TAG, "NTP sync completed; next sync in 24h");
    }

    vTaskDelay(pdMS_TO_TICKS(WIFI_SYNC_ATTEMPT_INTERVAL_MS));
  }
}

static const char *debug_wifi_ssid(void) {
  return config_drive_get_config()->wifi_ssid;
}

static const char *debug_wifi_pass(void) {
  return config_drive_get_config()->wifi_pass;
}

static uint32_t debug_free_heap(void) {
  return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
}

static void power_off_handler(void) {
  ESP_LOGI(TAG, "Auto-shutdown: 15 min inactivity with no timer running");
  if (display_power_off()) {
    /* Release the LVGL mutex before spinning: power_off_handler is called from
     * tick_cb which runs inside lv_timer_handler under display_lock. Without
     * this, the LVGL task holds the mutex indefinitely while waiting for the
     * device to power down. When charging, the device never powers off, so the
     * mutex would be held forever and the screen would freeze. */
    display_unlock();
    for (;;) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
  ESP_LOGW(TAG, "Auto-shutdown: power_off failed, staying on");
}

void app_main(void) {
  ESP_LOGI(TAG, "Pomodoro ESP32-S3 startup");

  /* Enable dynamic frequency scaling and light sleep between tasks. */
  esp_pm_config_t pm_config = {
      .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
      .min_freq_mhz = 80,
      .light_sleep_enable = true,
  };
  if (esp_pm_configure(&pm_config) != ESP_OK) {
    ESP_LOGW(TAG, "Power management configure failed — running at fixed clock");
  }

  if (!display_power_hold_enable()) {
    ESP_LOGW(
        TAG,
        "Early power-hold latch failed; device may turn off on PWR release");
  }

  if (!storage_init()) {
    ESP_LOGE(TAG, "Storage init failed");
  }

  if (!rtc_pcf85063_init()) {
    ESP_LOGW(TAG, "RTC init failed, continuing without RTC");
  }

  if (!battery_init()) {
    ESP_LOGW(TAG, "Battery ADC init failed, battery percentage unavailable");
  }
  app_set_battery_percent_provider(battery_get_percentage);
  app_set_battery_charging_provider(battery_is_charging);
  app_set_brightness_provider(display_set_brightness);
  app_set_bell_volume_provider(audio_set_bell_volume);
  app_set_power_off_provider(power_off_handler);
  app_set_wifi_ssid_provider(debug_wifi_ssid);
  app_set_wifi_pass_provider(debug_wifi_pass);
  app_set_ip_addr_provider(wifi_sync_get_ip_str);
  app_set_free_heap_provider(debug_free_heap);

  /* Try to restore system time from RTC before Wi-Fi comes up */
  struct tm rtc_time = {0};
  if (rtc_pcf85063_get_time(&rtc_time)) {
    time_t t = mktime(&rtc_time);
    struct timeval tv = {.tv_sec = t};
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "System time restored from RTC");
  }

  /* Config drive: read TOMATO32_CONFIG.conf and start TinyUSB CDC + MSC.
   * This must run before wifi_sync_init() so runtime credentials are
   * available, and before display_init() so TinyUSB owns the USB peripheral
   * before LVGL starts touching shared resources. */
  config_drive_init();
  const config_drive_config_t *cfg = config_drive_get_config();
  wifi_sync_set_credentials(cfg->wifi_ssid, cfg->wifi_pass, cfg->tz);

  lv_display_t *disp = display_init();
  assert(disp);

  if (display_lock(-1)) {
    display_show_startup_screen(
        "Tomato32", config_drive_usb_active() ? "Config mode" : "Starting...");
    lv_timer_handler();
    display_unlock();
  }

  /* audio_init must run after display_init: io_expander_init (inside
     display_init) creates the shared I2C bus on port 0 (SDA=47, SCL=48) and may
     power up the ES8311 codec via TCA9554. codec_init then reuses that bus via
     i2c_master_get_bus_handle rather than trying to create a second one. */
  if (!audio_init()) {
    ESP_LOGW(TAG, "Audio init failed");
  }

  if (display_lock(-1)) {
    app_init(disp);
    display_unlock();
  }

  if (button_input_init()) {
    xTaskCreatePinnedToCore(button_event_task, "button_evt", 4096, NULL, 3,
                            NULL, 1);
  } else {
    ESP_LOGE(TAG, "Button input init failed");
  }

  /* Start Wi-Fi and NTP in background; will update RTC when sync succeeds */
  wifi_sync_init();
  xTaskCreatePinnedToCore(wifi_resync_task, "wifi_resync", 3072, NULL, 2, NULL,
                          1);

  ESP_LOGI(TAG, "Setup complete");
}
