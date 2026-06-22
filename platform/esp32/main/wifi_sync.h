#ifndef WIFI_SYNC_H
#define WIFI_SYNC_H

#include <stdbool.h>

/*
 * Override Wi-Fi credentials and timezone at runtime (e.g. from config drive).
 * Call before wifi_sync_init(). Overrides compile-time values from
 * TOMATO32_CONFIG.txt at runtime. Pass NULL to leave a value unchanged.
 */
void wifi_sync_set_credentials(const char *ssid, const char *pass,
                               const char *tz);

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

#endif
