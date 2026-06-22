#include "config_drive.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_console.h"
#include "tinyusb_msc.h"
#include "wear_levelling.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "config_drive";
static const char *MOUNT_POINT = "/config";
static const char *CONFIG_FILE = "/config/TOMATO32_CONFIG.conf";

/* GPIO0 is the BOOT button (active low). Holding it at power-on requests
 * USB config drive mode. */
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define BOOT_BUTTON_LEVEL 0

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static config_drive_config_t s_config;
static bool s_usb_active = false;

static void trim_trailing(char *s) {
  size_t len = strlen(s);
  while (len > 0 &&
         (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ')) {
    s[--len] = '\0';
  }
}

static void write_default_config(void) {
  FILE *f = fopen(CONFIG_FILE, "w");
  if (!f) {
    ESP_LOGE(TAG, "Could not create %s", CONFIG_FILE);
    return;
  }
  fprintf(f, "# Tomato32 configuration\n"
             "# Edit this file, safely eject the drive, then power-cycle to "
             "apply.\n"
             "#\n"
             "# POSIX TZ format examples:\n"
             "#   UTC0\n"
             "#   JST-9\n"
             "#   CET-1CEST,M3.5.0/2,M10.5.0/3\n"
             "\n"
             "WIFI_SSID=\n"
             "WIFI_PASS=\n"
             "TZ=UTC0\n");
  fclose(f);
  ESP_LOGI(TAG, "Wrote default config file");
}

static void parse_config(void) {
  FILE *f = fopen(CONFIG_FILE, "r");
  if (!f) {
    ESP_LOGW(TAG, "Config file not found — creating defaults");
    write_default_config();
    return;
  }
  char line[192];
  while (fgets(line, sizeof(line), f)) {
    trim_trailing(line);
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }
    char *eq = strchr(line, '=');
    if (!eq) {
      continue;
    }
    *eq = '\0';
    const char *key = line;
    const char *val = eq + 1;
    if (strcmp(key, "WIFI_SSID") == 0) {
      strlcpy(s_config.wifi_ssid, val, sizeof(s_config.wifi_ssid));
    } else if (strcmp(key, "WIFI_PASS") == 0) {
      strlcpy(s_config.wifi_pass, val, sizeof(s_config.wifi_pass));
    } else if (strcmp(key, "TZ") == 0) {
      strlcpy(s_config.tz, val, sizeof(s_config.tz));
    }
  }
  fclose(f);
  ESP_LOGI(TAG, "Config loaded: SSID='%s' TZ='%s'", s_config.wifi_ssid,
           s_config.tz);
}

bool config_drive_usb_active(void) { return s_usb_active; }

bool config_drive_init(void) {
  memset(&s_config, 0, sizeof(s_config));
  strlcpy(s_config.tz, "UTC0", sizeof(s_config.tz));
  s_usb_active = false;

  /* ------------------------------------------------------------------ */
  /* Check whether USB config drive mode was requested.                 */
  /* The user holds the BOOT button (GPIO0, active-low) at power-on.   */
  /* ------------------------------------------------------------------ */
  gpio_config_t boot_io = {
      .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&boot_io);
  vTaskDelay(pdMS_TO_TICKS(5)); /* let the pull-up settle */
  bool usb_requested = (gpio_get_level(BOOT_BUTTON_GPIO) == BOOT_BUTTON_LEVEL);

  if (!usb_requested) {
    /* ---------------------------------------------------------------- */
    /* Normal boot: mount FAT briefly to read config, then unmount.     */
    /* TinyUSB is never started — no USB overhead at all.              */
    /* ---------------------------------------------------------------- */
    ESP_LOGI(TAG, "Normal boot — USB config drive inactive");
    esp_vfs_fat_mount_config_t fat_cfg = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_POINT, "config",
                                                     &fat_cfg, &s_wl_handle);
    if (err == ESP_OK) {
      parse_config();
      esp_vfs_fat_spiflash_unmount_rw_wl(MOUNT_POINT, s_wl_handle);
      s_wl_handle = WL_INVALID_HANDLE;
    } else {
      ESP_LOGW(TAG, "FAT mount failed (%s) — using default config",
               esp_err_to_name(err));
    }
    return true;
  }

  /* ------------------------------------------------------------------ */
  /* USB config drive mode: full TinyUSB CDC + MSC init.               */
  /* ------------------------------------------------------------------ */
  ESP_LOGI(TAG, "BOOT held — USB config drive active");

  bool msc_ok = false;

  do {
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "config");
    if (!part) {
      ESP_LOGE(TAG, "\"config\" partition not found — was the full firmware "
                    "flashed (not just the app)?");
      break;
    }

    if (wl_mount(part, &s_wl_handle) != ESP_OK) {
      ESP_LOGE(TAG, "wl_mount failed");
      break;
    }

    tinyusb_msc_driver_config_t msc_drv = {
        .user_flags.val = 0, .callback = NULL, .callback_arg = NULL};
    if (tinyusb_msc_install_driver(&msc_drv) != ESP_OK) {
      ESP_LOGE(TAG, "tinyusb_msc_install_driver failed");
      wl_unmount(s_wl_handle);
      s_wl_handle = WL_INVALID_HANDLE;
      break;
    }

    tinyusb_msc_storage_config_t msc_cfg = {
        .medium.wl_handle = s_wl_handle,
        .fat_fs =
            {
                .base_path = (char *)MOUNT_POINT,
                .config =
                    {
                        .format_if_mount_failed = true,
                        .max_files = 4,
                        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
                    },
                .do_not_format = false,
                .format_flags = 0,
            },
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
    };
    if (tinyusb_msc_new_storage_spiflash(&msc_cfg, NULL) != ESP_OK) {
      ESP_LOGE(TAG, "tinyusb_msc_new_storage_spiflash failed");
      tinyusb_msc_uninstall_driver();
      wl_unmount(s_wl_handle);
      s_wl_handle = WL_INVALID_HANDLE;
      break;
    }

    parse_config();
    msc_ok = true;
  } while (0);

  tinyusb_config_cdcacm_t cdc_cfg = {
      .cdc_port = TINYUSB_CDC_ACM_0,
      .callback_rx = NULL,
      .callback_rx_wanted_char = NULL,
      .callback_line_state_changed = NULL,
      .callback_line_coding_changed = NULL,
  };
  esp_err_t err = tinyusb_cdcacm_init(&cdc_cfg);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "tinyusb_cdcacm_init: %s", esp_err_to_name(err));
  }

  tinyusb_config_t tusb_cfg = {
      .port = TINYUSB_PORT_FULL_SPEED_0,
      .phy = {.skip_setup = false,
              .self_powered = false,
              .vbus_monitor_io = -1},
      .task = {.size = 4096, .priority = 5, .xCoreID = 0},
      .descriptor = {0},
      .event_cb = NULL,
      .event_arg = NULL,
  };
  err = tinyusb_driver_install(&tusb_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "tinyusb_driver_install: %s — no USB interface available",
             esp_err_to_name(err));
    return false;
  }

  if (tinyusb_console_init(0) != ESP_OK) {
    ESP_LOGW(TAG, "Console redirect to CDC failed; use UART0 for logs");
  }

  s_usb_active = true;
  return msc_ok;
}

const config_drive_config_t *config_drive_get_config(void) { return &s_config; }
