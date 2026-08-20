#include "display.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_io_expander.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_axs15231b.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <inttypes.h>

extern lv_font_t inter_36;
extern lv_font_t inter_20;

/* Pin configuration from Waveshare examples */
#define LCD_HOST SPI3_HOST
#define PIN_LCD_CS GPIO_NUM_9
#define PIN_LCD_PCLK GPIO_NUM_10
#define PIN_LCD_DATA0 GPIO_NUM_11
#define PIN_LCD_DATA1 GPIO_NUM_12
#define PIN_LCD_DATA2 GPIO_NUM_13
#define PIN_LCD_DATA3 GPIO_NUM_14
#define PIN_LCD_RST GPIO_NUM_21
#define PIN_LCD_BL GPIO_NUM_8

#define LCD_H_RES 172
#define LCD_V_RES 640
#define LCD_BIT_PER_PIXEL 16
#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (LCD_H_RES * LCD_V_RES * BYTES_PER_PIXEL)
#define DMA_BUFF_LEN (LCD_H_RES * 64 * 2)

#define TOUCH_SCL GPIO_NUM_18
#define TOUCH_SDA GPIO_NUM_17
#define TOUCH_ADDR 0x3B

/* Tick resolution kept at 5 ms for accurate indev/gesture timing.
 * With CONFIG_PM_ENABLE the CPU still light-sleeps between wakeups; the
 * incremental gain from a larger tick period does not justify the loss of
 * touch-timing accuracy in LVGL's indev state machine. */
#define LVGL_TICK_PERIOD_MS 5
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 10
#define LVGL_TASK_STACK_SIZE (8 * 1024)
#define LVGL_TASK_PRIORITY 4

/* LVGL can render directly in the byte order required by the panel.  This
 * removes one byte-swap operation per pixel from the flush path without
 * changing the amount of data sent over QSPI or the power-management policy.
 * Set to 0 only when testing a platform/driver that does not support the
 * swapped RGB565 draw target. */
#ifndef TOMATO32_DISPLAY_USE_SWAPPED_RGB565
#define TOMATO32_DISPLAY_USE_SWAPPED_RGB565 1
#endif

/* Optional one-second display timing report.  Enable with
 * -DTOMATO32_DISPLAY_PERF=1 for hardware profiling; the default build has no
 * timing calls or logging overhead. */
#ifndef TOMATO32_DISPLAY_PERF
#define TOMATO32_DISPLAY_PERF 0
#endif

static const char *TAG = "display";

#define KEY_INPUT_CANDIDATE_MASK                                               \
  (IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_3 |     \
   IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5)

static SemaphoreHandle_t lvgl_mux = NULL;
static SemaphoreHandle_t flush_done_semaphore = NULL;
static uint16_t *trans_buf_1 = NULL;
static uint16_t *trans_buf_2 = NULL;
static i2c_master_bus_handle_t touch_i2c_bus = NULL;
static i2c_master_dev_handle_t touch_dev = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t s_panel_io = NULL;
static esp_io_expander_handle_t io_expander_handle = NULL;
static volatile bool s_settings_key_event = false;
static bool s_touch_key_latched = false;
static uint8_t s_last_key_debug[8] = {0};
static bool s_last_key_debug_valid = false;
static lv_obj_t *s_startup_scr = NULL;
static lv_obj_t *s_startup_title_lbl = NULL;
static lv_obj_t *s_startup_subtitle_lbl = NULL;

#if TOMATO32_DISPLAY_PERF
typedef struct {
  int64_t last_flush_done_us;
  int64_t report_start_us;
  uint64_t flush_time_us;
  uint32_t frame_count;
  uint32_t deadline_misses;
  uint32_t max_flush_us;
} display_perf_state_t;

static display_perf_state_t s_display_perf;
#endif

static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 100},
    {0x29, (uint8_t[]){0x00}, 0, 100},
};

#if TOMATO32_DISPLAY_PERF
static int64_t display_perf_flush_start(void) { return esp_timer_get_time(); }

static void display_perf_flush_done(int64_t start_us) {
  int64_t now_us = esp_timer_get_time();
  uint32_t flush_us = (uint32_t)(now_us - start_us);

  if (s_display_perf.report_start_us == 0) {
    s_display_perf.report_start_us = now_us;
  }
  if (s_display_perf.last_flush_done_us != 0 &&
      now_us - s_display_perf.last_flush_done_us > 16000) {
    s_display_perf.deadline_misses++;
  }
  s_display_perf.last_flush_done_us = now_us;
  s_display_perf.flush_time_us += flush_us;
  s_display_perf.frame_count++;
  if (flush_us > s_display_perf.max_flush_us) {
    s_display_perf.max_flush_us = flush_us;
  }

  if (now_us - s_display_perf.report_start_us >= 1000000) {
    uint32_t avg_flush_us = s_display_perf.frame_count
                                ? (uint32_t)(s_display_perf.flush_time_us /
                                             s_display_perf.frame_count)
                                : 0;
    ESP_LOGI(TAG,
             "perf: frames=%" PRIu32 " avg_flush=%" PRIu32
             "us max_flush=%" PRIu32 "us deadline_misses=%" PRIu32,
             s_display_perf.frame_count, avg_flush_us,
             s_display_perf.max_flush_us, s_display_perf.deadline_misses);
    s_display_perf.report_start_us = now_us;
    s_display_perf.flush_time_us = 0;
    s_display_perf.frame_count = 0;
    s_display_perf.deadline_misses = 0;
    s_display_perf.max_flush_us = 0;
  }
}
#else
static int64_t display_perf_flush_start(void) { return 0; }
static void display_perf_flush_done(int64_t start_us) { (void)start_us; }
#endif

/* Fused 90° CW rotation and optional RGB565 byte-swap for one SPI chunk.
 *
 * The naive lv_draw_sw_rotate90 reads the source column-by-column, causing
 * one SPIRAM cache miss per pixel (≈110 k misses × 150 ns ≈ 16 ms) plus a
 * separate byte-swap pass over the whole frame.  This function reads the
 * source row-by-row (sequential SPIRAM access) and byte-swaps in the same
 * pass, cutting SPIRAM cache misses from ~110 k to ~172 per chunk.
 *
 *   src        logical framebuffer in SPIRAM  (src_w × src_h, RGB565)
 *   dst        DMA-capable buffer (chunk_lines × src_h uint16s)
 *   src_w      logical width  (640)
 *   src_h      logical height (172)
 *   row_start  first physical row of this chunk (multiple of lines_per_chunk)
 *   chunk_lines physical rows in this chunk
 *
 * Mapping (90° CW): phys(col=src_y, row=src_w-1-src_x) ← src(src_x, src_y)
 */
static IRAM_ATTR void rotate90_swap_chunk(const uint16_t *src, uint16_t *dst,
                                          int32_t src_w, int32_t src_h,
                                          int32_t row_start,
                                          int32_t chunk_lines) {
  int32_t x_end = (src_w - 1) - row_start;
  int32_t x_start = x_end - (chunk_lines - 1);
  for (int32_t src_y = 0; src_y < src_h; src_y++) {
    const uint16_t *srow = src + (src_y * src_w);
    for (int32_t src_x = x_start; src_x <= x_end; src_x++) {
      uint16_t px = srow[src_x];
#if !TOMATO32_DISPLAY_USE_SWAPPED_RGB565
      px = (uint16_t)((px >> 8) | (px << 8));
#endif
      dst[((src_w - 1 - src_x) - row_start) * src_h + src_y] = px;
    }
  }
}

static bool
example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                esp_lcd_panel_io_event_data_t *edata,
                                void *user_ctx) {
  BaseType_t high_task_awoken = pdFALSE;
  xSemaphoreGiveFromISR(flush_done_semaphore, &high_task_awoken);
  return false;
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                                  uint8_t *color_p) {
  int64_t perf_start_us = display_perf_flush_start();
  esp_lcd_panel_handle_t panel =
      (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);

  lv_display_rotation_t rotation = lv_display_get_rotation(disp);

  if (rotation != LV_DISPLAY_ROTATION_90) {
    /* Non-rotated fallback: byte-swap in place and send in chunks. */
#if !TOMATO32_DISPLAY_USE_SWAPPED_RGB565
    lv_draw_sw_rgb565_swap(color_p,
                           lv_area_get_width(area) * lv_area_get_height(area));
#endif
    int32_t tx_w = lv_area_get_width(area);
    size_t bytes_per_line = (size_t)tx_w * BYTES_PER_PIXEL;
    int32_t lpc = (int32_t)(DMA_BUFF_LEN / bytes_per_line);
    if (lpc < 1)
      lpc = 1;
    int32_t y = area->y1;
    const uint8_t *map = color_p;
    xSemaphoreGive(flush_done_semaphore);
    while (y <= area->y2) {
      int32_t remaining = area->y2 - y + 1;
      int32_t cl = remaining > lpc ? lpc : remaining;
      size_t chunk_bytes = bytes_per_line * (size_t)cl;
      xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
      memcpy(trans_buf_1, map, chunk_bytes);
      esp_lcd_panel_draw_bitmap(panel, area->x1, y, area->x2 + 1, y + cl,
                                trans_buf_1);
      y += cl;
      map += chunk_bytes;
    }
    xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
    display_perf_flush_done(perf_start_us);
    lv_disp_flush_ready(disp);
    return;
  }

  /* ROTATION_90 fast path --------------------------------------------------
   * Chunk-fused rotate+swap: each DMA buffer is filled by reading the SPIRAM
   * source row-by-row (sequential → ~172 cache misses per chunk vs. ~11 000
   * in the old column-major approach).  Double-buffered so CPU rotation of
   * chunk N+1 overlaps with SPI transfer of chunk N.                       */
  lv_area_t rotated_area = *area;
  lv_display_rotate_area(disp, &rotated_area);

  int32_t src_w = lv_area_get_width(area);
  int32_t src_h = lv_area_get_height(area);
  int32_t phys_w = lv_area_get_width(&rotated_area);
  int32_t lpc = (int32_t)(DMA_BUFF_LEN / ((size_t)phys_w * BYTES_PER_PIXEL));
  if (lpc < 1)
    lpc = 1;

  const uint16_t *src = (const uint16_t *)color_p;
  uint16_t *bufs[2] = {trans_buf_1, trans_buf_2};
  int bi = 0;
  bool spi_pending = false;
  int32_t row = rotated_area.y1;

  while (row <= rotated_area.y2) {
    int32_t remaining = rotated_area.y2 - row + 1;
    int32_t cl = remaining > lpc ? lpc : remaining;

    rotate90_swap_chunk(src, bufs[bi], src_w, src_h, row, cl);

    if (spi_pending) {
      xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
    }
    esp_lcd_panel_draw_bitmap(panel, rotated_area.x1, row, rotated_area.x2 + 1,
                              row + cl, bufs[bi]);
    spi_pending = true;
    bi ^= 1;
    row += cl;
  }
  if (spi_pending) {
    xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
  }
  display_perf_flush_done(perf_start_us);
  lv_disp_flush_ready(disp);
}

static void touch_input_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  (void)indev;
  uint8_t read_touchpad_cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0,
                                   0x0,  0x0e, 0x0,  0x0,  0x0};
  uint8_t buff[32] = {0};

  if (i2c_master_transmit_receive(touch_dev, read_touchpad_cmd, 11, buff, 32,
                                  pdMS_TO_TICKS(15)) != ESP_OK) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  if (buff[1] == 0) {
    bool changed = !s_last_key_debug_valid;
    for (int i = 0; i < 8 && !changed; i++) {
      if (s_last_key_debug[i] != buff[i]) {
        changed = true;
      }
    }
    if (changed) {
      memcpy(s_last_key_debug, buff, 8);
      s_last_key_debug_valid = true;
      ESP_LOGI(TAG,
               "Touch status bytes: b0=%u b1=%u b2=0x%02x b3=0x%02x b4=0x%02x "
               "b5=0x%02x b6=0x%02x b7=0x%02x",
               buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6],
               buff[7]);
    }
  }

  /*
   * Some panel variants report bezel keys via touch status bytes, not GPIO.
   * Treat these flags as a one-shot settings key event.
   */
  bool hw_key_active = false;
  if (buff[1] == 0) {
    if ((buff[4] & 0x80) != 0 || (buff[5] & 0x80) != 0) {
      hw_key_active = true;
    }
  }
  if (hw_key_active && !s_touch_key_latched) {
    s_touch_key_latched = true;
    s_settings_key_event = true;
    ESP_LOGI(TAG, "Touch key event detected (b1=%u b4=0x%02x b5=0x%02x)",
             buff[1], buff[4], buff[5]);
  } else if (!hw_key_active) {
    s_touch_key_latched = false;
  }

  uint16_t pointX = (((uint16_t)buff[2] & 0x0f) << 8) | (uint16_t)buff[3];
  uint16_t pointY = (((uint16_t)buff[4] & 0x0f) << 8) | (uint16_t)buff[5];

  if (buff[1] > 0 && buff[1] < 5) {
    data->state = LV_INDEV_STATE_PRESSED;
    if (pointX > LCD_V_RES)
      pointX = LCD_V_RES;
    if (pointY > LCD_H_RES)
      pointY = LCD_H_RES;
    /* Map to native panel coordinates (172 x 640) */
    data->point.x = pointY;
    data->point.y = LCD_V_RES - pointX;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static void example_increase_lvgl_tick(void *arg) {
  lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

bool display_lock(int timeout_ms) {
  const TickType_t timeout_ticks =
      (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

void display_unlock(void) { xSemaphoreGive(lvgl_mux); }

static void example_lvgl_port_task(void *arg) {
  uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
  for (;;) {
    if (display_lock(-1)) {
      task_delay_ms = lv_timer_handler();
      display_unlock();
    }
    if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
      task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
      task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
    }
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}

static void lcd_bl_init(void) {
  ledc_timer_config_t timer_conf = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_8_BIT,
      .timer_num = LEDC_TIMER_3,
      .freq_hz = 50 * 1000,
      .clk_cfg = LEDC_SLOW_CLK_RC_FAST,
  };
  ledc_channel_config_t ledc_conf = {
      .gpio_num = PIN_LCD_BL,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LEDC_CHANNEL_1,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER_3,
      .duty = 0xFF,
      .hpoint = 0,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_timer_config(&timer_conf));
  ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_channel_config(&ledc_conf));
}

void display_set_brightness(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  /* The AXS15231B integrates display and touch on the same IC. DCS sleep-in
   * (0x10) would disable the touch controller too, preventing tap-to-wake.
   * Backlight PWM alone is used for power saving at 0% brightness. */

  uint32_t duty;
  if (percent == 0) {
    /* Full off: drive active-low backlight transistor fully off. */
    duty = 0xFFU;
  } else {
    /* Remap user-visible 10-100% to the hardware-viable 40-100% range.
     * The panel's backlight LED driver cuts out below ~40% PWM low-time. */
    if (percent < 10)
      percent = 10;
    uint32_t hw_percent = 40U + ((uint32_t)(percent - 10U) * 60U) / 90U;

    /* This panel's backlight transistor is active-low on PWM duty. */
    duty = 0xFFU - (hw_percent * 0xFFU) / 100U;
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
}

static void lcd_reset(void) {
  gpio_config_t gpio_conf = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = ((uint64_t)0x01 << PIN_LCD_RST),
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&gpio_conf));

  ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_RST, 1));
  vTaskDelay(pdMS_TO_TICKS(30));
  ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_RST, 0));
  vTaskDelay(pdMS_TO_TICKS(250));
  ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_RST, 1));
  vTaskDelay(pdMS_TO_TICKS(30));
}

static void io_expander_init(void) {
  i2c_master_bus_handle_t tca9554_bus = NULL;

  /* Reuse I2C0 if another module has already created it. */
  esp_err_t r = i2c_master_get_bus_handle(0, &tca9554_bus);
  if (r != ESP_OK || !tca9554_bus) {
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = GPIO_NUM_48,
        .sda_io_num = GPIO_NUM_47,
        .glitch_ignore_cnt = 7,
        .flags = {.enable_internal_pullup = true},
    };
    r = i2c_new_master_bus(&bus_cfg, &tca9554_bus);
    if (r != ESP_OK) {
      ESP_LOGE(TAG, "Failed to get/create TCA9554 I2C bus (%d)", r);
      return;
    }
  }

  /* Try TCA9554 (0x20) first, then TCA9554A (0x38) as fallback */
  uint32_t tca_addr = ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000; /* 0x20 */
  if (esp_io_expander_new_i2c_tca9554(tca9554_bus, tca_addr,
                                      &io_expander_handle) != ESP_OK) {
    ESP_LOGW(TAG, "TCA9554 not found at 0x20, trying TCA9554A at 0x38");
    tca_addr = ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000; /* 0x38 */
    if (esp_io_expander_new_i2c_tca9554(tca9554_bus, tca_addr,
                                        &io_expander_handle) != ESP_OK) {
      ESP_LOGE(
          TAG,
          "TCA9554/A not found at 0x20 or 0x38 — NS4168 amp will be DISABLED");
      return;
    }
  }
  ESP_LOGI(TAG, "TCA9554 found at 0x%02" PRIx32, tca_addr);

  r = esp_io_expander_set_dir(io_expander_handle, IO_EXPANDER_PIN_NUM_1,
                              IO_EXPANDER_OUTPUT);
  if (r != ESP_OK)
    ESP_LOGW(TAG, "set_dir pin1 ret %d", r);
  /* Keep panel/backlight related enable pin asserted. */
  r = esp_io_expander_set_level(io_expander_handle, IO_EXPANDER_PIN_NUM_1, 1);
  if (r != ESP_OK)
    ESP_LOGW(TAG, "set_level pin1 ret %d", r);

  /* Keep VBAT power-hold path asserted by default. */
  r = esp_io_expander_set_dir(io_expander_handle, IO_EXPANDER_PIN_NUM_6,
                              IO_EXPANDER_OUTPUT);
  if (r != ESP_OK)
    ESP_LOGW(TAG, "set_dir pin6 (power hold) ret %d", r);
  r = esp_io_expander_set_level(io_expander_handle, IO_EXPANDER_PIN_NUM_6, 1);
  if (r != ESP_OK)
    ESP_LOGW(TAG, "set_level pin6 HIGH (power hold) ret %d", r);

  /* Candidate physical keys input lines (active-low on this board family). */
  r = esp_io_expander_set_dir(io_expander_handle, KEY_INPUT_CANDIDATE_MASK,
                              IO_EXPANDER_INPUT);
  if (r != ESP_OK)
    ESP_LOGW(TAG, "set_dir key candidates ret %d", r);

  /* Keep NS4168 amplifier off until playback is requested. */
  r = esp_io_expander_set_dir(io_expander_handle, IO_EXPANDER_PIN_NUM_7,
                              IO_EXPANDER_OUTPUT);
  if (r != ESP_OK)
    ESP_LOGE(TAG, "set_dir pin7 (amp SD) ret %d", r);
  r = esp_io_expander_set_level(io_expander_handle, IO_EXPANDER_PIN_NUM_7, 0);
  if (r != ESP_OK)
    ESP_LOGE(TAG, "set_level pin7 LOW (amp SD) ret %d", r);

  if (r == ESP_OK)
    ESP_LOGI(TAG, "NS4168 amp SD (TCA9554 pin 7) set LOW — amp disabled");
}

bool display_get_pressed_key_mask(uint32_t *pressed_mask) {
  if (!io_expander_handle || !pressed_mask) {
    return false;
  }

  uint32_t level_mask = KEY_INPUT_CANDIDATE_MASK;
  if (esp_io_expander_get_level(io_expander_handle, KEY_INPUT_CANDIDATE_MASK,
                                &level_mask) != ESP_OK) {
    return false;
  }

  *pressed_mask = (~level_mask) & KEY_INPUT_CANDIDATE_MASK;
  return true;
}

bool display_consume_settings_key_event(void) {
  bool fired = s_settings_key_event;
  s_settings_key_event = false;
  return fired;
}

bool display_amp_enable(void) {
  if (!io_expander_handle) {
    ESP_LOGW(TAG, "Amp enable skipped: no IO expander handle");
    return false;
  }

  esp_err_t r = esp_io_expander_set_dir(
      io_expander_handle, IO_EXPANDER_PIN_NUM_7, IO_EXPANDER_OUTPUT);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "display_amp_enable: set_dir pin7 failed (%d)", r);
    return false;
  }
  r = esp_io_expander_set_level(io_expander_handle, IO_EXPANDER_PIN_NUM_7, 1);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "display_amp_enable: set_level pin7 failed (%d)", r);
    return false;
  }
  return true;
}

bool display_amp_disable(void) {
  if (!io_expander_handle) {
    ESP_LOGW(TAG, "Amp disable skipped: no IO expander handle");
    return false;
  }

  esp_err_t r = esp_io_expander_set_dir(
      io_expander_handle, IO_EXPANDER_PIN_NUM_7, IO_EXPANDER_OUTPUT);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "display_amp_disable: set_dir pin7 failed (%d)", r);
    return false;
  }
  r = esp_io_expander_set_level(io_expander_handle, IO_EXPANDER_PIN_NUM_7, 0);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "display_amp_disable: set_level pin7 failed (%d)", r);
    return false;
  }
  return true;
}

bool display_power_hold_enable(void) {
  if (!io_expander_handle) {
    io_expander_init();
  }

  if (!io_expander_handle) {
    ESP_LOGW(TAG, "Power-hold enable skipped: no IO expander handle");
    return false;
  }

  esp_err_t r = esp_io_expander_set_dir(
      io_expander_handle, IO_EXPANDER_PIN_NUM_6, IO_EXPANDER_OUTPUT);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "display_power_hold_enable: set_dir pin6 failed (%d)", r);
    return false;
  }

  r = esp_io_expander_set_level(io_expander_handle, IO_EXPANDER_PIN_NUM_6, 1);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "display_power_hold_enable: set_level pin6 HIGH failed (%d)",
             r);
    return false;
  }

  ESP_LOGI(TAG, "Power-hold asserted (TCA9554 pin6 HIGH)");
  return true;
}

bool display_power_off(void) {
  if (!io_expander_handle) {
    ESP_LOGW(TAG, "Power-off skipped: no IO expander handle");
    return false;
  }

  esp_err_t r = esp_io_expander_set_dir(
      io_expander_handle, IO_EXPANDER_PIN_NUM_6, IO_EXPANDER_OUTPUT);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "display_power_off: set_dir pin6 failed (%d)", r);
    return false;
  }

  r = esp_io_expander_set_level(io_expander_handle, IO_EXPANDER_PIN_NUM_6, 0);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "display_power_off: set_level pin6 LOW failed (%d)", r);
    return false;
  }

  ESP_LOGI(TAG, "Power-off request sent (TCA9554 pin6 LOW)");
  return true;
}

static void touch_init(void) {
  i2c_master_bus_config_t bus_cfg = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_NUM_1,
      .scl_io_num = TOUCH_SCL,
      .sda_io_num = TOUCH_SDA,
      .glitch_ignore_cnt = 7,
      .flags = {.enable_internal_pullup = true},
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &touch_i2c_bus));

  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = TOUCH_ADDR,
      .scl_speed_hz = 300000,
  };
  ESP_ERROR_CHECK(
      i2c_master_bus_add_device(touch_i2c_bus, &dev_cfg, &touch_dev));
}

void display_show_startup_screen(const char *title, const char *subtitle) {
  if (!s_startup_scr) {
    s_startup_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_startup_scr);
    lv_obj_set_style_bg_opa(s_startup_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_startup_scr, lv_color_hex(0x000000), 0);

    s_startup_title_lbl = lv_label_create(s_startup_scr);
    lv_obj_set_style_text_color(s_startup_title_lbl, lv_color_hex(0xEAF2FF), 0);
    lv_obj_set_style_text_font(s_startup_title_lbl, &inter_36, 0);
    lv_obj_align(s_startup_title_lbl, LV_ALIGN_CENTER, 0, -16);

    s_startup_subtitle_lbl = lv_label_create(s_startup_scr);
    lv_obj_set_style_text_color(s_startup_subtitle_lbl, lv_color_hex(0x9FB3C8),
                                0);
    lv_obj_set_style_text_font(s_startup_subtitle_lbl, &inter_20, 0);
    lv_obj_align(s_startup_subtitle_lbl, LV_ALIGN_CENTER, 0, 26);
  }

  lv_label_set_text(s_startup_title_lbl, title ? title : "Tomato32");
  lv_label_set_text(s_startup_subtitle_lbl,
                    subtitle ? subtitle : "Starting...");
  lv_scr_load(s_startup_scr);
}

lv_display_t *display_init(void) {
  io_expander_init();
  lcd_reset();
  touch_init();

  flush_done_semaphore = xSemaphoreCreateBinary();
  assert(flush_done_semaphore);

  ESP_LOGI(TAG, "Initialize SPI bus");
  spi_bus_config_t buscfg = {
      .sclk_io_num = PIN_LCD_PCLK,
      .data0_io_num = PIN_LCD_DATA0,
      .data1_io_num = PIN_LCD_DATA1,
      .data2_io_num = PIN_LCD_DATA2,
      .data3_io_num = PIN_LCD_DATA3,
      .max_transfer_sz = DMA_BUFF_LEN,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  ESP_LOGI(TAG, "Install panel IO");
  s_panel_io = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = PIN_LCD_CS,
      .dc_gpio_num = -1,
      .spi_mode = 3,
      .pclk_hz = 40 * 1000 * 1000,
      .trans_queue_depth = 10,
      .on_color_trans_done = example_notify_lvgl_flush_ready,
      .lcd_cmd_bits = 32,
      .lcd_param_bits = 8,
      .flags = {.quad_mode = true},
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &s_panel_io));

  axs15231b_vendor_config_t vendor_config = {
      .flags = {.use_qspi_interface = 1},
      .init_cmds = lcd_init_cmds,
      .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
  };
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = -1,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .bits_per_pixel = LCD_BIT_PER_PIXEL,
      .vendor_config = &vendor_config,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_axs15231b(s_panel_io, &panel_config, &panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

  ESP_LOGI(TAG, "Initialize LVGL");
  lv_init();
  lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
#if TOMATO32_DISPLAY_USE_SWAPPED_RGB565
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
#endif
  lv_display_set_flush_cb(disp, example_lvgl_flush_cb);

  uint8_t *buf1 = heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  uint8_t *buf2 = heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  trans_buf_1 = (uint16_t *)heap_caps_malloc(DMA_BUFF_LEN, MALLOC_CAP_DMA);
  trans_buf_2 = (uint16_t *)heap_caps_malloc(DMA_BUFF_LEN, MALLOC_CAP_DMA);
  assert(buf1 && buf2 && trans_buf_1 && trans_buf_2);

  lv_display_set_buffers(disp, buf1, buf2, BUFF_SIZE,
                         LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_user_data(disp, panel_handle);
  /* Keep rotation in the flush path.  The AXS15231B QSPI driver does not
   * provide a reliable drop-in hardware axis swap for this board, and the
   * software path also keeps the existing touch coordinate mapping intact. */
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);

  /* Push a clean black frame before enabling backlight to avoid power-on
   * artifacts. */
  lv_obj_t *boot_scr = lv_scr_act();
  lv_obj_set_style_bg_opa(boot_scr, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(boot_scr, lv_color_hex(0x000000), 0);
  lv_obj_invalidate(boot_scr);
  lv_timer_handler();

  lcd_bl_init();
  display_set_brightness(80);

  /* Touch input */
  lv_indev_t *touch_indev = lv_indev_create();
  lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch_indev, touch_input_read_cb);
  lv_indev_set_display(touch_indev, disp);

  /* Tick timer */
  esp_timer_create_args_t tick_args = {
      .callback = &example_increase_lvgl_tick,
      .name = "lvgl_tick",
  };
  esp_timer_handle_t tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000));

  lvgl_mux = xSemaphoreCreateMutex();
  assert(lvgl_mux);
  xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE,
                          NULL, LVGL_TASK_PRIORITY, NULL, 0);

  return disp;
}
