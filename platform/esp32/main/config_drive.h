#pragma once

#include <stdbool.h>
#include <stddef.h>

#define CONFIG_DRIVE_MAX_WIFI_NETWORKS 8
#define CONFIG_DRIVE_WIFI_SSID_SIZE 64
#define CONFIG_DRIVE_WIFI_PASS_SIZE 64

typedef struct {
  char ssid[CONFIG_DRIVE_WIFI_SSID_SIZE];
  char pass[CONFIG_DRIVE_WIFI_PASS_SIZE];
} config_drive_wifi_network_t;

typedef struct {
  config_drive_wifi_network_t wifi_networks[CONFIG_DRIVE_MAX_WIFI_NETWORKS];
  size_t wifi_network_count;
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
 * Only complete SSID/password pairs are included in wifi_networks.
 */
const config_drive_config_t *config_drive_get_config(void);
