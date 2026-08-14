#ifndef WIFI_SYNC_H
#define WIFI_SYNC_H

#include "config_drive.h"
#include <stdbool.h>

/*
 * Copy Wi-Fi credentials and timezone at runtime (e.g. from the config drive).
 * Call before wifi_sync_init().
 */
void wifi_sync_set_credentials(const config_drive_config_t *config);

/*
 * Initialize Wi-Fi/NTP stack in STA mode without connecting.
 * Sync is triggered on demand via wifi_sync_request_sync().
 */
void wifi_sync_init(void);

/*
 * Block until NTP has synced at least once (or timeout).
 * Returns true if sync succeeded.
 */
bool wifi_sync_wait_for_ntp(int timeout_ms);

/*
 * Trigger an NTP sync cycle on demand.
 * Wi-Fi will be started if needed. Returns true on successful sync.
 */
bool wifi_sync_request_sync(int timeout_ms);

/*
 * Returns the last known IP address as a dotted-decimal string, or an empty
 * string if Wi-Fi has not yet connected. Valid after wifi_sync_init().
 */
const char *wifi_sync_get_ip_str(void);

/* Returns the configured/selected network shown on the debug screen. */
const char *wifi_sync_get_ssid(void);
const char *wifi_sync_get_pass(void);

#endif
