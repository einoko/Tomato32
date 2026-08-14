#include "wifi_sync.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "rtc_pcf85063.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "wifi_sync";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define NTP_SYNCED_BIT BIT1
#define WIFI_STARTED_BIT BIT2
#define WIFI_CONNECT_FAILED_BIT BIT3
static bool s_keep_wifi_running = true;
static bool s_wifi_started = false;
static bool s_wifi_config_valid = false;

/* Runtime credentials copied from the config drive. */
static config_drive_wifi_network_t
    s_runtime_networks[CONFIG_DRIVE_MAX_WIFI_NETWORKS];
static size_t s_runtime_network_count = 0;
static char s_runtime_tz[64] = {0};
static char s_selected_ssid[CONFIG_DRIVE_WIFI_SSID_SIZE] = {0};
static char s_selected_pass[CONFIG_DRIVE_WIFI_PASS_SIZE] = {0};
static char s_ip_str[24] = {0};

void wifi_sync_set_credentials(const config_drive_config_t *config) {
  memset(s_runtime_networks, 0, sizeof(s_runtime_networks));
  s_runtime_network_count = 0;
  s_runtime_tz[0] = '\0';

  if (!config) {
    return;
  }

  s_runtime_network_count = config->wifi_network_count;
  if (s_runtime_network_count > CONFIG_DRIVE_MAX_WIFI_NETWORKS) {
    s_runtime_network_count = CONFIG_DRIVE_MAX_WIFI_NETWORKS;
  }
  memcpy(s_runtime_networks, config->wifi_networks,
         s_runtime_network_count * sizeof(s_runtime_networks[0]));
  strlcpy(s_runtime_tz, config->tz, sizeof(s_runtime_tz));
}

#ifndef WIFI_SYNC_KEEP_CONNECTED_AFTER_NTP
#define WIFI_SYNC_KEEP_CONNECTED_AFTER_NTP 0
#endif

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  (void)arg;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    xEventGroupSetBits(s_wifi_event_group, WIFI_STARTED_BIT);
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    if (!s_keep_wifi_running) {
      ESP_LOGI(TAG, "Wi-Fi disconnected (intentional stop)");
      return;
    }
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECT_FAILED_BIT);
    ESP_LOGW(TAG, "Wi-Fi connection attempt failed");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

    /* Start SNTP once we have IP */
    esp_sntp_stop();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
  }
}

static void wifi_sync_stop_for_power_saving(void) {
  s_keep_wifi_running = false;
  esp_sntp_stop();

  if (!s_wifi_started) {
    return;
  }

  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_disconnect failed (%d)", err);
  }

  err = esp_wifi_stop();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_stop failed (%d)", err);
  } else {
    s_wifi_started = false;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    xEventGroupClearBits(s_wifi_event_group, WIFI_STARTED_BIT);
    ESP_LOGI(TAG, "Wi-Fi stopped for battery savings");
  }
}

static void time_sync_notification_cb(struct timeval *tv) {
  (void)tv;
  ESP_LOGI(TAG, "NTP sync received");

  time_t now = time(NULL);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  if (rtc_pcf85063_set_time(&timeinfo)) {
    ESP_LOGI(TAG, "RTC updated from NTP (UTC)");
  } else {
    ESP_LOGE(TAG, "NTP updated system clock, but RTC update failed");
  }

  xEventGroupSetBits(s_wifi_event_group, NTP_SYNCED_BIT);

#if !WIFI_SYNC_KEEP_CONNECTED_AFTER_NTP
  wifi_sync_stop_for_power_saving();
#endif
}

void wifi_sync_init(void) {
  s_wifi_event_group = xEventGroupCreate();

  const char *tz = (s_runtime_tz[0] != '\0') ? s_runtime_tz : "UTC0";

  if (s_runtime_network_count == 0) {
    ESP_LOGW(TAG, "Wi-Fi credentials not configured. Set WIFI_SSID_1 and "
                  "WIFI_PASS_1 in TOMATO32_CONFIG.conf on the TOMATO32 drive.");
    s_wifi_config_valid = false;
    return;
  }

  setenv("TZ", tz, 1);
  tzset();

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  s_keep_wifi_running = false;
  s_wifi_started = false;
  s_wifi_config_valid = true;
  s_selected_ssid[0] = '\0';
  s_selected_pass[0] = '\0';

  sntp_set_time_sync_notification_cb(time_sync_notification_cb);

  ESP_LOGI(TAG, "Wi-Fi STA init done (%u network(s), on-demand sync only)",
           (unsigned)s_runtime_network_count);
}

bool wifi_sync_wait_for_ntp(int timeout_ms) {
  EventBits_t bits =
      xEventGroupWaitBits(s_wifi_event_group, NTP_SYNCED_BIT, pdFALSE, pdTRUE,
                          pdMS_TO_TICKS(timeout_ms));
  return (bits & NTP_SYNCED_BIT) != 0;
}

typedef struct {
  size_t network_index;
  int8_t rssi;
  uint8_t channel;
  uint8_t bssid[6];
} wifi_candidate_t;

static bool scan_ssid_matches(const uint8_t *scanned_ssid, size_t scanned_size,
                              const char *configured_ssid) {
  size_t configured_len = strlen(configured_ssid);
  size_t scanned_len = strnlen((const char *)scanned_ssid, scanned_size);
  return scanned_len == configured_len &&
         memcmp(scanned_ssid, configured_ssid, configured_len) == 0;
}

static bool wifi_scan_candidates(wifi_candidate_t *candidates,
                                 size_t *candidate_count) {
  *candidate_count = 0;
  wifi_scan_config_t scan_config = {
      .scan_type = WIFI_SCAN_TYPE_ACTIVE,
      .show_hidden = false,
  };
  esp_err_t err = esp_wifi_scan_start(&scan_config, true);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Wi-Fi scan failed (%d)", err);
    return false;
  }

  uint16_t ap_count = 0;
  err = esp_wifi_scan_get_ap_num(&ap_count);
  if (err != ESP_OK || ap_count == 0) {
    ESP_LOGI(TAG, "Wi-Fi scan found no access points");
    return err == ESP_OK;
  }

  wifi_ap_record_t *records = calloc(ap_count, sizeof(*records));
  if (!records) {
    ESP_LOGE(TAG, "Could not allocate %u Wi-Fi scan records", ap_count);
    return false;
  }

  uint16_t record_count = ap_count;
  err = esp_wifi_scan_get_ap_records(&record_count, records);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Could not read Wi-Fi scan records (%d)", err);
    free(records);
    return false;
  }

  *candidate_count = 0;
  for (size_t network_index = 0; network_index < s_runtime_network_count;
       network_index++) {
    bool found = false;
    wifi_candidate_t best = {0};
    for (uint16_t record_index = 0; record_index < record_count;
         record_index++) {
      wifi_ap_record_t *record = &records[record_index];
      if (!scan_ssid_matches(record->ssid, sizeof(record->ssid),
                             s_runtime_networks[network_index].ssid)) {
        continue;
      }
      if (!found || record->rssi > best.rssi) {
        best.network_index = network_index;
        best.rssi = record->rssi;
        best.channel = record->primary;
        memcpy(best.bssid, record->bssid, sizeof(best.bssid));
        found = true;
      }
    }
    if (found) {
      candidates[(*candidate_count)++] = best;
    }
  }
  free(records);

  /* Strongest matching AP first; preserve config-file order for ties. */
  for (size_t i = 0; i < *candidate_count; i++) {
    for (size_t j = i + 1; j < *candidate_count; j++) {
      if (candidates[j].rssi > candidates[i].rssi) {
        wifi_candidate_t tmp = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = tmp;
      }
    }
  }
  return true;
}

static int remaining_timeout_ms(int64_t deadline_us) {
  int64_t remaining_us = deadline_us - esp_timer_get_time();
  if (remaining_us <= 0) {
    return 0;
  }
  int64_t remaining_ms = (remaining_us + 999) / 1000;
  return remaining_ms > INT32_MAX ? INT32_MAX : (int)remaining_ms;
}

static bool wifi_try_candidate(const wifi_candidate_t *candidate,
                               int64_t deadline_us) {
  const config_drive_wifi_network_t *network =
      &s_runtime_networks[candidate->network_index];
  wifi_config_t wifi_config = {
      .sta = {.threshold.authmode = WIFI_AUTH_WPA2_PSK,
              .channel = candidate->channel,
              .bssid_set = true},
  };
  strlcpy((char *)wifi_config.sta.ssid, network->ssid,
          sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, network->pass,
          sizeof(wifi_config.sta.password));
  memcpy(wifi_config.sta.bssid, candidate->bssid,
         sizeof(wifi_config.sta.bssid));

  strlcpy(s_selected_ssid, network->ssid, sizeof(s_selected_ssid));
  strlcpy(s_selected_pass, network->pass, sizeof(s_selected_pass));
  ESP_LOGI(TAG, "Trying Wi-Fi network '%s' (RSSI %d)", network->ssid,
           candidate->rssi);

  xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | NTP_SYNCED_BIT |
                                               WIFI_CONNECT_FAILED_BIT);
  esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Could not configure Wi-Fi network '%s' (%d)", network->ssid,
             err);
    return false;
  }

  err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Could not start Wi-Fi connection to '%s' (%d)",
             network->ssid, err);
    return false;
  }

  int wait_ms = remaining_timeout_ms(deadline_us);
  if (wait_ms == 0) {
    return false;
  }
  EventBits_t bits = xEventGroupWaitBits(
      s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_CONNECT_FAILED_BIT, pdFALSE,
      pdFALSE, pdMS_TO_TICKS(wait_ms));
  if ((bits & WIFI_CONNECTED_BIT) == 0) {
    return false;
  }

  wait_ms = remaining_timeout_ms(deadline_us);
  if (wait_ms == 0 || !wifi_sync_wait_for_ntp(wait_ms)) {
    if (s_keep_wifi_running) {
      esp_wifi_disconnect();
      xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECT_FAILED_BIT, pdFALSE,
                          pdFALSE, pdMS_TO_TICKS(2000));
    }
    return false;
  }
  return true;
}

bool wifi_sync_request_sync(int timeout_ms) {
  if (!s_wifi_event_group) {
    return false;
  }

  if (!s_wifi_config_valid) {
    ESP_LOGW(TAG, "NTP sync skipped: Wi-Fi is not configured");
    return false;
  }

  xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | NTP_SYNCED_BIT);
  s_keep_wifi_running = true;
  s_selected_ssid[0] = '\0';
  s_selected_pass[0] = '\0';

  if (!s_wifi_started) {
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_wifi_start failed (%d)", err);
      return false;
    }
    s_wifi_started = true;
    EventBits_t bits =
        xEventGroupWaitBits(s_wifi_event_group, WIFI_STARTED_BIT, pdFALSE,
                            pdTRUE, pdMS_TO_TICKS(1000));
    if ((bits & WIFI_STARTED_BIT) == 0) {
      ESP_LOGW(TAG, "Timed out waiting for Wi-Fi driver start");
      wifi_sync_stop_for_power_saving();
      return false;
    }
  }

  wifi_candidate_t candidates[CONFIG_DRIVE_MAX_WIFI_NETWORKS];
  size_t candidate_count = 0;
  if (!wifi_scan_candidates(candidates, &candidate_count)) {
    wifi_sync_stop_for_power_saving();
    return false;
  }
  if (candidate_count == 0) {
    ESP_LOGI(TAG, "No configured Wi-Fi network is currently visible");
    wifi_sync_stop_for_power_saving();
    return false;
  }

  int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
  bool synced = false;
  for (size_t i = 0; i < candidate_count; i++) {
    if (remaining_timeout_ms(deadline_us) == 0) {
      break;
    }
    if (wifi_try_candidate(&candidates[i], deadline_us)) {
      synced = true;
      break;
    }
  }

#if !WIFI_SYNC_KEEP_CONNECTED_AFTER_NTP
  if (!synced) {
    wifi_sync_stop_for_power_saving();
  }
#endif

  return synced;
}

const char *wifi_sync_get_ip_str(void) { return s_ip_str; }

const char *wifi_sync_get_ssid(void) {
  if (s_selected_ssid[0] != '\0') {
    return s_selected_ssid;
  }
  return s_runtime_network_count > 0 ? s_runtime_networks[0].ssid : "";
}

const char *wifi_sync_get_pass(void) {
  if (s_selected_pass[0] != '\0') {
    return s_selected_pass;
  }
  return s_runtime_network_count > 0 ? s_runtime_networks[0].pass : "";
}
