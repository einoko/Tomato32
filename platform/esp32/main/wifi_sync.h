#ifndef WIFI_SYNC_H
#define WIFI_SYNC_H

#include <stdbool.h>

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
