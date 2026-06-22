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
#include <inttypes.h>
#include <string.h>
#include <time.h>

static const char *TAG = "wifi_sync";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define NTP_SYNCED_BIT BIT1
static bool s_keep_wifi_running = true;
static bool s_wifi_started = false;
static bool s_wifi_config_valid = false;

/* Runtime credential overrides (set via wifi_sync_set_credentials()). */
static char s_runtime_ssid[64] = {0};
static char s_runtime_pass[64] = {0};
static char s_runtime_tz[64] = {0};
static char s_ip_str[24] = {0};

void wifi_sync_set_credentials(const char *ssid, const char *pass,
                               const char *tz)
{
  if (ssid)
    strlcpy(s_runtime_ssid, ssid, sizeof(s_runtime_ssid));
  if (pass)
    strlcpy(s_runtime_pass, pass, sizeof(s_runtime_pass));
  if (tz)
    strlcpy(s_runtime_tz, tz, sizeof(s_runtime_tz));
}

/* Exponential backoff for reconnect attempts. Starts at 500 ms, doubles each
 * failure up to a 30-second cap.  Reset on every successful connection or when
 * a new sync cycle begins. */
#define WIFI_RECONNECT_DELAY_MIN_MS 500
#define WIFI_RECONNECT_DELAY_MAX_MS 30000
static uint32_t s_reconnect_delay_ms = WIFI_RECONNECT_DELAY_MIN_MS;
static esp_timer_handle_t s_reconnect_timer = NULL;

static void reconnect_timer_cb(void *arg)
{
  (void)arg;
  if (s_keep_wifi_running)
  {
    esp_wifi_connect();
  }
}

#ifndef WIFI_SYNC_KEEP_CONNECTED_AFTER_NTP
#define WIFI_SYNC_KEEP_CONNECTED_AFTER_NTP 0
#endif

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
  (void)arg;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT &&
           event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    if (!s_keep_wifi_running)
    {
      ESP_LOGI(TAG, "Wi-Fi disconnected (intentional stop)");
      return;
    }
    ESP_LOGW(TAG, "Wi-Fi disconnected, retrying in %" PRIu32 "ms",
             s_reconnect_delay_ms);
    esp_timer_start_once(s_reconnect_timer,
                         (uint64_t)s_reconnect_delay_ms * 1000ULL);
    s_reconnect_delay_ms = s_reconnect_delay_ms < WIFI_RECONNECT_DELAY_MAX_MS
                               ? s_reconnect_delay_ms * 2
                               : WIFI_RECONNECT_DELAY_MAX_MS;
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    /* Successful connection — reset backoff for the next sync cycle. */
    s_reconnect_delay_ms = WIFI_RECONNECT_DELAY_MIN_MS;
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

static void wifi_sync_stop_for_power_saving(void)
{
  s_keep_wifi_running = false;
  s_reconnect_delay_ms = WIFI_RECONNECT_DELAY_MIN_MS; /* reset for next cycle */
  if (s_reconnect_timer)
  {
    esp_timer_stop(s_reconnect_timer); /* cancel any pending retry */
  }
  esp_sntp_stop();

  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "esp_wifi_disconnect failed (%d)", err);
  }

  err = esp_wifi_stop();
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "esp_wifi_stop failed (%d)", err);
  }
  else
  {
    s_wifi_started = false;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    ESP_LOGI(TAG, "Wi-Fi stopped for battery savings");
  }
}

static void time_sync_notification_cb(struct timeval *tv)
{
  (void)tv;
  ESP_LOGI(TAG, "NTP sync received");

  time_t now = time(NULL);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  if (rtc_pcf85063_set_time(&timeinfo))
  {
    ESP_LOGI(TAG, "RTC updated from NTP");
  }

  xEventGroupSetBits(s_wifi_event_group, NTP_SYNCED_BIT);

#if !WIFI_SYNC_KEEP_CONNECTED_AFTER_NTP
  wifi_sync_stop_for_power_saving();
#endif
}

void wifi_sync_init(void)
{
  s_wifi_event_group = xEventGroupCreate();

  const char *ssid = s_runtime_ssid;
  const char *pass = s_runtime_pass;
  const char *tz = (s_runtime_tz[0] != '\0') ? s_runtime_tz : "UTC0";

  if (strlen(ssid) == 0 || strlen(pass) == 0)
  {
    ESP_LOGW(TAG, "Wi-Fi credentials not configured. Set WIFI_SSID and "
                  "WIFI_PASS in TOMATO32_CONFIG.conf on the TOMATO32 drive.");
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

  wifi_config_t wifi_config = {
      .sta = {.threshold.authmode = WIFI_AUTH_WPA2_PSK},
  };
  strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, pass,
          sizeof(wifi_config.sta.password));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  s_keep_wifi_running = false;
  s_wifi_started = false;
  s_wifi_config_valid = true;

  esp_timer_create_args_t reconnect_args = {
      .callback = reconnect_timer_cb,
      .name = "wifi_reconnect",
  };
  ESP_ERROR_CHECK(esp_timer_create(&reconnect_args, &s_reconnect_timer));

  sntp_set_time_sync_notification_cb(time_sync_notification_cb);

  ESP_LOGI(TAG, "Wi-Fi STA init done (idle, on-demand sync only)");
}

bool wifi_sync_wait_for_ntp(int timeout_ms)
{
  EventBits_t bits = xEventGroupWaitBits(
      s_wifi_event_group, WIFI_CONNECTED_BIT | NTP_SYNCED_BIT, pdFALSE, pdTRUE,
      pdMS_TO_TICKS(timeout_ms));
  return (bits & (WIFI_CONNECTED_BIT | NTP_SYNCED_BIT)) ==
         (WIFI_CONNECTED_BIT | NTP_SYNCED_BIT);
}

bool wifi_sync_request_sync(int timeout_ms)
{
  if (!s_wifi_event_group)
  {
    return false;
  }

  if (!s_wifi_config_valid)
  {
    ESP_LOGW(TAG, "NTP sync skipped: Wi-Fi is not configured");
    return false;
  }

  xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | NTP_SYNCED_BIT);
  s_keep_wifi_running = true;
  s_reconnect_delay_ms = WIFI_RECONNECT_DELAY_MIN_MS; /* fresh attempt */

  if (!s_wifi_started)
  {
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "esp_wifi_start failed (%d)", err);
      return false;
    }
    s_wifi_started = true;
  }
  else
  {
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "esp_wifi_connect returned %d", err);
    }
  }

  bool synced = wifi_sync_wait_for_ntp(timeout_ms);

#if !WIFI_SYNC_KEEP_CONNECTED_AFTER_NTP
  if (!synced)
  {
    wifi_sync_stop_for_power_saving();
  }
#endif

  return synced;
}

const char *wifi_sync_get_ip_str(void) { return s_ip_str; }
