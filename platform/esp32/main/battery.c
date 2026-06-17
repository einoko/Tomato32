#include "battery.h"

#include <math.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"

/* Derived from Waveshare ESP-IDF ADC example for ESP32-S3-Touch-LCD-3.49. */
#define BATTERY_ADC_UNIT ADC_UNIT_1
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_3
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_12
#define BATTERY_ADC_BITWIDTH ADC_BITWIDTH_12

#define BATTERY_DIVIDER_RATIO 3.0f
#define BATTERY_CELL_EMPTY_MV 3000
#define BATTERY_CELL_FULL_MV 4200
#define BATTERY_ADC_SAMPLES 8
#define BATTERY_MEASURE_CACHE_US 500000
#define BATTERY_CHARGE_WINDOW_US 10000000
#define BATTERY_CHARGE_RISE_MV 10
#define BATTERY_CHARGE_DROP_MV -20
#define BATTERY_NEAR_FULL_MV 4180
#define BATTERY_NEAR_FULL_FALL_MV -6

static const char *TAG = "battery";

static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_cali_handle;
static bool s_ready;
static bool s_has_cali;
static bool s_charging_sample_valid;
static bool s_is_charging;
static int s_last_battery_mv;
static int64_t s_last_sample_us;
static bool s_measure_valid;
static int s_last_measure_battery_mv;
static int64_t s_last_measure_us;

typedef struct {
  int mv;
  int pct;
} battery_soc_point_t;

static const battery_soc_point_t s_soc_table[] = {
    {3000, 0},  {3300, 5},  {3450, 10}, {3550, 20}, {3650, 35},  {3750, 50},
    {3850, 65}, {3950, 78}, {4050, 90}, {4150, 97}, {4200, 100},
};

static bool battery_read_mv(int *out_mv) {
  if (!s_ready || !out_mv) {
    return false;
  }

  int raw = 0;
  if (adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
    return false;
  }

  if (s_has_cali) {
    if (adc_cali_raw_to_voltage(s_cali_handle, raw, out_mv) != ESP_OK) {
      return false;
    }
  } else {
    *out_mv = (raw * 3300) / 4095;
  }

  return true;
}

static int battery_percent_from_mv(int battery_mv) {
  size_t i = 0;
  size_t last = sizeof(s_soc_table) / sizeof(s_soc_table[0]) - 1;

  if (battery_mv <= s_soc_table[0].mv) {
    return 0;
  }
  if (battery_mv >= s_soc_table[last].mv) {
    return 100;
  }

  for (i = 1; i <= last; ++i) {
    if (battery_mv <= s_soc_table[i].mv) {
      int x0 = s_soc_table[i - 1].mv;
      int y0 = s_soc_table[i - 1].pct;
      int x1 = s_soc_table[i].mv;
      int y1 = s_soc_table[i].pct;
      float t = (float)(battery_mv - x0) / (float)(x1 - x0);
      int pct = (int)lroundf((float)y0 + t * (float)(y1 - y0));
      if (pct < 0) {
        return 0;
      }
      if (pct > 100) {
        return 100;
      }
      return pct;
    }
  }

  return 100;
}

static bool battery_read_cell_mv(int *out_battery_mv) {
  int i;
  int64_t now_us;
  int valid = 0;
  int adc_sum_mv = 0;

  if (!out_battery_mv) {
    return false;
  }

  now_us = esp_timer_get_time();
  if (s_measure_valid &&
      (now_us - s_last_measure_us) < BATTERY_MEASURE_CACHE_US) {
    *out_battery_mv = s_last_measure_battery_mv;
    return true;
  }

  for (i = 0; i < BATTERY_ADC_SAMPLES; ++i) {
    int adc_mv = 0;
    if (!battery_read_mv(&adc_mv)) {
      continue;
    }
    adc_sum_mv += adc_mv;
    ++valid;
  }

  if (valid <= 0) {
    return false;
  }

  int adc_avg_mv = (int)lroundf((float)adc_sum_mv / (float)valid);
  int battery_mv = (int)lroundf((float)adc_avg_mv * BATTERY_DIVIDER_RATIO);

  if (battery_mv < BATTERY_CELL_EMPTY_MV) {
    battery_mv = BATTERY_CELL_EMPTY_MV;
  }
  if (battery_mv > BATTERY_CELL_FULL_MV) {
    battery_mv = BATTERY_CELL_FULL_MV;
  }

  s_last_measure_battery_mv = battery_mv;
  s_last_measure_us = now_us;
  s_measure_valid = true;
  *out_battery_mv = battery_mv;
  return true;
}

bool battery_init(void) {
  adc_oneshot_unit_init_cfg_t unit_cfg = {
      .unit_id = BATTERY_ADC_UNIT,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &s_adc_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "adc_oneshot_new_unit failed (%d)", err);
    return false;
  }

  adc_oneshot_chan_cfg_t channel_cfg = {
      .atten = BATTERY_ADC_ATTEN,
      .bitwidth = BATTERY_ADC_BITWIDTH,
  };
  err = adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL,
                                   &channel_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "adc_oneshot_config_channel failed (%d)", err);
    return false;
  }

  adc_cali_curve_fitting_config_t cali_cfg = {
      .unit_id = BATTERY_ADC_UNIT,
      .atten = BATTERY_ADC_ATTEN,
      .bitwidth = BATTERY_ADC_BITWIDTH,
  };
  err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
  if (err == ESP_OK) {
    s_has_cali = true;
  } else {
    s_has_cali = false;
    ESP_LOGW(TAG, "ADC calibration unavailable (%d), using raw fallback", err);
  }

  s_ready = true;
  s_charging_sample_valid = false;
  s_is_charging = false;
  s_last_battery_mv = 0;
  s_last_sample_us = 0;
  s_measure_valid = false;
  s_last_measure_battery_mv = 0;
  s_last_measure_us = 0;
  ESP_LOGI(TAG, "Battery ADC ready (channel %d)", BATTERY_ADC_CHANNEL);
  return true;
}

int battery_get_percentage(void) {
  int battery_mv = 0;
  if (!battery_read_cell_mv(&battery_mv)) {
    return -1;
  }

  return battery_percent_from_mv(battery_mv);
}

bool battery_is_charging(void) {
  int battery_mv = 0;
  if (!battery_read_cell_mv(&battery_mv)) {
    return false;
  }

  int64_t now_us = esp_timer_get_time();

  if (!s_charging_sample_valid) {
    s_charging_sample_valid = true;
    s_last_battery_mv = battery_mv;
    s_last_sample_us = now_us;
    return false;
  }

  int64_t dt_us = now_us - s_last_sample_us;
  if (dt_us >= BATTERY_CHARGE_WINDOW_US) {
    int dv_mv = battery_mv - s_last_battery_mv;

    /* Conservative thresholds to avoid flicker from short load spikes. */
    if (dv_mv >= BATTERY_CHARGE_RISE_MV) {
      s_is_charging = true;
    } else if (dv_mv <= BATTERY_CHARGE_DROP_MV) {
      s_is_charging = false;
    } else if (battery_mv >= BATTERY_NEAR_FULL_MV &&
               dv_mv >= BATTERY_NEAR_FULL_FALL_MV) {
      s_is_charging = true;
    }

    s_last_battery_mv = battery_mv;
    s_last_sample_us = now_us;
  }

  return s_is_charging;
}