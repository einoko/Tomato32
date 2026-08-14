#include "rtc_pcf85063.h"
#include "codec_init.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <time.h>

#define RTC_I2C_PORT I2C_NUM_0
#define RTC_SCL_PIN GPIO_NUM_48
#define RTC_SDA_PIN GPIO_NUM_47
#define RTC_ADDR 0x51
#define RTC_SPEED_HZ 300000

/* Register map */
#define PCF85063_REG_CTRL1 0x00
#define PCF85063_REG_SC 0x04
#define PCF85063_REG_MN 0x05
#define PCF85063_REG_HR 0x06
#define PCF85063_REG_DM 0x07
#define PCF85063_REG_DW 0x08
#define PCF85063_REG_MO 0x09
#define PCF85063_REG_YR 0x0A

static const char *TAG = "rtc_pcf85063";
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t rtc_dev = NULL;

static uint8_t bcd2dec(uint8_t val) { return ((val >> 4) * 10) + (val & 0x0F); }

static uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }

static esp_err_t rtc_read_reg(uint8_t reg, uint8_t *buf, size_t len) {
  return i2c_master_transmit_receive(rtc_dev, &reg, 1, buf, len,
                                     pdMS_TO_TICKS(1000));
}

static esp_err_t rtc_write_reg(uint8_t reg, const uint8_t *buf, size_t len) {
  uint8_t tmp[16];
  if (len + 1 > sizeof(tmp)) {
    return ESP_ERR_INVALID_SIZE;
  }
  tmp[0] = reg;
  for (size_t i = 0; i < len; i++) {
    tmp[i + 1] = buf[i];
  }
  return i2c_master_transmit(rtc_dev, tmp, len + 1, pdMS_TO_TICKS(1000));
}

bool rtc_pcf85063_init(void) {
  /* Try to reuse I2C bus already created by codec_board (audio init) */
  i2c_bus = (i2c_master_bus_handle_t)get_i2c_bus_handle(RTC_I2C_PORT);
  if (!i2c_bus) {
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = RTC_I2C_PORT,
        .scl_io_num = RTC_SCL_PIN,
        .sda_io_num = RTC_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags =
            {
                .enable_internal_pullup = true,
            },
    };
    if (i2c_new_master_bus(&bus_cfg, &i2c_bus) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create I2C bus");
      return false;
    }
  }

  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = RTC_ADDR,
      .scl_speed_hz = RTC_SPEED_HZ,
  };

  if (i2c_master_bus_add_device(i2c_bus, &dev_cfg, &rtc_dev) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add RTC device");
    return false;
  }

  /* Ensure oscillator is running and select 24-hour mode, preserving the
   * crystal capacitor setting. */
  uint8_t ctrl1 = 0;
  if (rtc_read_reg(PCF85063_REG_CTRL1, &ctrl1, 1) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read Control_1");
    return false;
  }
  ctrl1 &= (uint8_t)~(0x20 | 0x02); /* STOP and 12_24 */
  if (rtc_write_reg(PCF85063_REG_CTRL1, &ctrl1, 1) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write Control_1");
    return false;
  }

  ESP_LOGI(TAG, "RTC initialized");
  return true;
}

bool rtc_pcf85063_get_time(struct tm *timeinfo) {
  if (!rtc_dev || !timeinfo) {
    return false;
  }

  uint8_t buf[7];
  if (rtc_read_reg(PCF85063_REG_SC, buf, 7) != ESP_OK) {
    return false;
  }

  if (buf[0] & 0x80) {
    ESP_LOGW(TAG, "RTC oscillator-stop flag is set; time is invalid");
    return false;
  }

  timeinfo->tm_sec = bcd2dec(buf[0] & 0x7F);
  timeinfo->tm_min = bcd2dec(buf[1] & 0x7F);
  timeinfo->tm_hour = bcd2dec(buf[2] & 0x3F);
  timeinfo->tm_mday = bcd2dec(buf[3] & 0x3F);
  timeinfo->tm_wday = bcd2dec(buf[4] & 0x07);
  timeinfo->tm_mon = bcd2dec(buf[5] & 0x1F) - 1;
  timeinfo->tm_year = bcd2dec(buf[6]) + 2000 - 1900;
  timeinfo->tm_isdst = -1;
  return true;
}

bool rtc_pcf85063_set_time(const struct tm *timeinfo) {
  if (!rtc_dev || !timeinfo) {
    return false;
  }

  uint8_t buf[7];
  buf[0] = dec2bcd(timeinfo->tm_sec) & 0x7F;
  buf[1] = dec2bcd(timeinfo->tm_min) & 0x7F;
  buf[2] = dec2bcd(timeinfo->tm_hour) & 0x3F;
  buf[3] = dec2bcd(timeinfo->tm_mday) & 0x3F;
  buf[4] = dec2bcd(timeinfo->tm_wday) & 0x07;
  buf[5] = dec2bcd(timeinfo->tm_mon + 1) & 0x1F;
  buf[6] = dec2bcd(timeinfo->tm_year + 1900 - 2000);

  /* Stop the divider while programming the calendar, then restart it. */
  uint8_t ctrl1 = 0;
  if (rtc_read_reg(PCF85063_REG_CTRL1, &ctrl1, 1) != ESP_OK) {
    return false;
  }
  uint8_t ctrl1_stop = ctrl1 | 0x20;
  if (rtc_write_reg(PCF85063_REG_CTRL1, &ctrl1_stop, 1) != ESP_OK) {
    return false;
  }

  if (rtc_write_reg(PCF85063_REG_SC, buf, 7) != ESP_OK) {
    uint8_t ctrl1_run = ctrl1 & (uint8_t)~0x20;
    (void)rtc_write_reg(PCF85063_REG_CTRL1, &ctrl1_run, 1);
    return false;
  }

  uint8_t ctrl1_run = ctrl1 & (uint8_t)~0x20;
  if (rtc_write_reg(PCF85063_REG_CTRL1, &ctrl1_run, 1) != ESP_OK) {
    return false;
  }

  ESP_LOGI(TAG, "RTC time set");
  return true;
}

bool rtc_pcf85063_is_running(void) {
  if (!rtc_dev) {
    return false;
  }
  uint8_t ctrl1 = 0;
  if (rtc_read_reg(PCF85063_REG_CTRL1, &ctrl1, 1) != ESP_OK) {
    return false;
  }
  uint8_t seconds = 0;
  if (rtc_read_reg(PCF85063_REG_SC, &seconds, 1) != ESP_OK) {
    return false;
  }
  return (ctrl1 & 0x20) == 0 && (seconds & 0x80) == 0;
}
