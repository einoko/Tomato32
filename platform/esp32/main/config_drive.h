#pragma once

#include <stdbool.h>

typedef struct {
  char wifi_ssid[64];
  char wifi_pass[64];
  char tz[64];
} config_drive_config_t;

/*
 * Initialize the config drive:
 *   - Mounts the FAT "config" partition and reads TOMATO32_CONFIG.conf.
 *   - Installs TinyUSB CDC + MSC so the drive is visible when connected to USB.
 *   - Redirects the serial console to USB CDC.
 * Must be called before wifi_sync_init().
 * Returns true on success; the config is available even if USB init fails.
 */
bool config_drive_init(void);

/*
 * Returns true if TinyUSB was started (BOOT was held at power-on).
 * Safe to call after config_drive_init().
 */
bool config_drive_usb_active(void);

/*
 * Return the parsed config values. Valid after config_drive_init() returns.
 * Empty strings mean "not configured".
 */
const config_drive_config_t *config_drive_get_config(void);
