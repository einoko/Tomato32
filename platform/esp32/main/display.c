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
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <inttypes.h>

extern lv_font_t inter_36;
extern lv_font_t inter_20;
extern lv_font_t inter_16;

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

/* 60 MHz is the next conservative step above the original 40 MHz setting.
 * Lower this to 40 MHz if the panel shows transfer artifacts. */
#ifndef TOMATO32_LCD_PCLK_HZ
#define TOMATO32_LCD_PCLK_HZ (60 * 1000 * 1000)
#endif

#define TOUCH_SCL GPIO_NUM_18
#define TOUCH_SDA GPIO_NUM_17
#define TOUCH_ADDR 0x3B

#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 10
#define LVGL_TASK_ANIM_MIN_DELAY_MS 2
#define LVGL_TASK_STACK_SIZE (8 * 1024)
#define LVGL_TASK_PRIORITY 4
#define FLUSH_TASK_STACK_SIZE (4 * 1024)
#define FLUSH_TASK_PRIORITY 4
#define FLUSH_TASK_CORE 1
#define TOUCH_RELEASE_DEBOUNCE_MS 30
/* Touch is polled over I2C; slow polling once the UI has been idle so the
 * CPU can stay in light sleep longer. */
#define TOUCH_POLL_ACTIVE_MS LV_DEF_REFR_PERIOD
#define TOUCH_POLL_IDLE_MS 50
#define TOUCH_POLL_IDLE_AFTER_MS 3000

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
/* Held while rendering/flushing so DFS doesn't drop to 80 MHz mid-frame. */
static esp_pm_lock_handle_t s_cpu_max_lock = NULL;
static lv_display_t *s_disp = NULL;
static bool s_backlight_off = false;

typedef struct {
  uint8_t *px_map;
  lv_area_t area;
  lv_area_t rotated_area;
  bool rotated;
} flush_job_t;

static QueueHandle_t s_flush_queue = NULL;
static SemaphoreHandle_t s_frame_done_semaphore = NULL;
/* AXS15231B serves both QSPI panel and I2C touch; keep them exclusive. */
static SemaphoreHandle_t s_panel_chip_mutex = NULL;
static volatile bool s_flush_in_flight = false;
static uint16_t *trans_buf_1 = NULL;
static uint16_t *trans_buf_2 = NULL;
static i2c_master_bus_handle_t touch_i2c_bus = NULL;
static i2c_master_dev_handle_t touch_dev = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t s_panel_io = NULL;
static esp_io_expander_handle_t io_expander_handle = NULL;
static volatile bool s_settings_key_event = false;
static bool s_touch_key_latched = false;
static bool s_touch_pressed = false;
static int64_t s_touch_release_start_us = 0;
static lv_point_t s_touch_last_point = {0};
static uint8_t s_last_key_debug[8] = {0};
static bool s_last_key_debug_valid = false;
static lv_obj_t *s_startup_scr = NULL;
static lv_obj_t *s_startup_title_lbl = NULL;
static lv_obj_t *s_startup_subtitle_lbl = NULL;
static lv_obj_t *s_startup_version_lbl = NULL;

#if TOMATO32_DISPLAY_PERF
typedef struct {
  int64_t last_flush_done_us;
  uint64_t flush_time_us;
  uint32_t frame_count;
  uint32_t deadline_misses;
  uint32_t max_flush_us;
} display_perf_state_t;

typedef enum {
  FLUSH_STAGE_IDLE,
  FLUSH_STAGE_WAIT_CHIP,
  FLUSH_STAGE_ROTATE,
  FLUSH_STAGE_DRAW,
  FLUSH_STAGE_DRAIN,
} flush_stage_t;

static display_perf_state_t s_display_perf;
static portMUX_TYPE s_display_perf_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile int64_t s_lvgl_last_run_us;
static volatile flush_stage_t s_flush_stage;
static volatile bool s_lvgl_in_frame_wait;
static volatile bool s_touch_in_i2c;
static volatile uint32_t s_touch_i2c_errors;
static volatile uint32_t s_touch_gaps_bridged;
static volatile uint32_t s_touch_releases;
static int64_t s_touch_last_release_us;
static int64_t s_touch_last_release_gap_us;
#define DISPLAY_PERF_SET(var, val) ((var) = (val))
#else
#define DISPLAY_PERF_SET(var, val) ((void)0)
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

  taskENTER_CRITICAL(&s_display_perf_lock);
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
  taskEXIT_CRITICAL(&s_display_perf_lock);
}

/* Reports even with no frames, so a stuck pipeline still shows its state. */
static void display_perf_report_task(void *arg) {
  (void)arg;
  static const char *const stage_names[] = {"idle", "wait_chip", "rotate",
                                            "draw", "drain"};
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    taskENTER_CRITICAL(&s_display_perf_lock);
    display_perf_state_t snap = s_display_perf;
    s_display_perf.flush_time_us = 0;
    s_display_perf.frame_count = 0;
    s_display_perf.deadline_misses = 0;
    s_display_perf.max_flush_us = 0;
    taskEXIT_CRITICAL(&s_display_perf_lock);

    uint32_t avg_flush_us =
        snap.frame_count ? (uint32_t)(snap.flush_time_us / snap.frame_count)
                         : 0;
    uint32_t lvgl_idle_ms =
        (uint32_t)((esp_timer_get_time() - s_lvgl_last_run_us) / 1000);
    ESP_LOGI(TAG,
             "perf: frames=%" PRIu32 " avg_flush=%" PRIu32
             "us max_flush=%" PRIu32 "us deadline_misses=%" PRIu32
             " lvgl_idle=%" PRIu32 "ms flush=%s in_flight=%d "
             "frame_wait=%d touch_i2c=%d touch_err=%" PRIu32
             " touch_gaps=%" PRIu32 " touch_releases=%" PRIu32,
             snap.frame_count, avg_flush_us, snap.max_flush_us,
             snap.deadline_misses, lvgl_idle_ms, stage_names[s_flush_stage],
             s_flush_in_flight, s_lvgl_in_frame_wait, s_touch_in_i2c,
             s_touch_i2c_errors, s_touch_gaps_bridged, s_touch_releases);
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

/* esp_lcd drains queued color transfers before any new command, so a
 * command-less tx_param waits for the bus without counting ISR completions. */
static void wait_transfers_done(void) {
  DISPLAY_PERF_SET(s_flush_stage, FLUSH_STAGE_DRAIN);
  esp_lcd_panel_io_tx_param(s_panel_io, -1, NULL, 0);
}

static void draw_chunk(int x1, int y1, int x2, int y2, const void *buf) {
  DISPLAY_PERF_SET(s_flush_stage, FLUSH_STAGE_DRAW);
  esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, buf);
}

static void flush_send_unrotated(const flush_job_t *job) {
  const lv_area_t *area = &job->area;
  /* Non-rotated fallback: byte-swap in place and send in chunks. */
#if !TOMATO32_DISPLAY_USE_SWAPPED_RGB565
  lv_draw_sw_rgb565_swap(job->px_map,
                         lv_area_get_width(area) * lv_area_get_height(area));
#endif
  int32_t tx_w = lv_area_get_width(area);
  size_t bytes_per_line = (size_t)tx_w * BYTES_PER_PIXEL;
  int32_t lpc = (int32_t)(DMA_BUFF_LEN / bytes_per_line);
  if (lpc < 1)
    lpc = 1;
  int32_t y = area->y1;
  const uint8_t *map = job->px_map;
  uint16_t *bufs[2] = {trans_buf_1, trans_buf_2};
  int bi = 0;
  while (y <= area->y2) {
    int32_t remaining = area->y2 - y + 1;
    int32_t cl = remaining > lpc ? lpc : remaining;
    size_t chunk_bytes = bytes_per_line * (size_t)cl;
    memcpy(bufs[bi], map, chunk_bytes);
    draw_chunk(area->x1, y, area->x2 + 1, y + cl, bufs[bi]);
    bi ^= 1;
    y += cl;
    map += chunk_bytes;
  }
  wait_transfers_done();
}

/* Chunk-fused rotate+swap: each DMA buffer is filled by reading the SPIRAM
 * source row-by-row (sequential → ~172 cache misses per chunk vs. ~11 000
 * in the old column-major approach).  Double-buffered so CPU rotation of
 * chunk N+1 overlaps with SPI transfer of chunk N; draw_chunk(N+1) waits for
 * chunk N inside esp_lcd, so a buffer is never refilled while in flight. */
static void flush_send_rotated90(const flush_job_t *job) {
  const lv_area_t *rotated_area = &job->rotated_area;
  int32_t src_w = lv_area_get_width(&job->area);
  int32_t src_h = lv_area_get_height(&job->area);
  int32_t phys_w = lv_area_get_width(rotated_area);
  int32_t lpc = (int32_t)(DMA_BUFF_LEN / ((size_t)phys_w * BYTES_PER_PIXEL));
  if (lpc < 1)
    lpc = 1;

  const uint16_t *src = (const uint16_t *)job->px_map;
  uint16_t *bufs[2] = {trans_buf_1, trans_buf_2};
  int bi = 0;
  int32_t row = rotated_area->y1;

  while (row <= rotated_area->y2) {
    int32_t remaining = rotated_area->y2 - row + 1;
    int32_t cl = remaining > lpc ? lpc : remaining;

    DISPLAY_PERF_SET(s_flush_stage, FLUSH_STAGE_ROTATE);
    rotate90_swap_chunk(src, bufs[bi], src_w, src_h, row, cl);
    draw_chunk(rotated_area->x1, row, rotated_area->x2 + 1, row + cl, bufs[bi]);
    bi ^= 1;
    row += cl;
  }
  wait_transfers_done();
}

/* Runs on the other core so LVGL can render the next frame into the second
 * draw buffer while this one is rotated and sent. */
static void flush_task(void *arg) {
  (void)arg;
  flush_job_t job;
  for (;;) {
    xQueueReceive(s_flush_queue, &job, portMAX_DELAY);
    if (s_cpu_max_lock)
      esp_pm_lock_acquire(s_cpu_max_lock);
    DISPLAY_PERF_SET(s_flush_stage, FLUSH_STAGE_WAIT_CHIP);
    xSemaphoreTake(s_panel_chip_mutex, portMAX_DELAY);
    int64_t perf_start_us = display_perf_flush_start();
    if (job.rotated) {
      flush_send_rotated90(&job);
    } else {
      flush_send_unrotated(&job);
    }
    display_perf_flush_done(perf_start_us);
    xSemaphoreGive(s_panel_chip_mutex);
    DISPLAY_PERF_SET(s_flush_stage, FLUSH_STAGE_IDLE);
    s_flush_in_flight = false;
    if (s_cpu_max_lock)
      esp_pm_lock_release(s_cpu_max_lock);
    xSemaphoreGive(s_frame_done_semaphore);
  }
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                                  uint8_t *color_p) {
  flush_job_t job = {
      .px_map = color_p,
      .area = *area,
      .rotated_area = *area,
      .rotated = lv_display_get_rotation(disp) == LV_DISPLAY_ROTATION_90,
  };
  if (job.rotated) {
    lv_display_rotate_area(disp, &job.rotated_area);
  }
  s_flush_in_flight = true;
  xQueueSend(s_flush_queue, &job, portMAX_DELAY);
}

/* LVGL clears its own flushing flag after this returns, so flush_task never
 * calls lv_display_flush_ready(). */
static void example_lvgl_flush_wait_cb(lv_display_t *disp) {
  (void)disp;
  DISPLAY_PERF_SET(s_lvgl_in_frame_wait, true);
  xSemaphoreTake(s_frame_done_semaphore, portMAX_DELAY);
  DISPLAY_PERF_SET(s_lvgl_in_frame_wait, false);
}

/* Block until the in-flight frame reaches the panel, leaving the done signal
 * for LVGL's next flush_wait_cb. */
static void display_wait_flush_idle(void) {
  if (s_flush_in_flight) {
    xSemaphoreTake(s_frame_done_semaphore, portMAX_DELAY);
    xSemaphoreGive(s_frame_done_semaphore);
  }
}

static void touch_report_pressed(lv_indev_data_t *data, int32_t x, int32_t y) {
#if TOMATO32_DISPLAY_PERF
  int64_t now_us = esp_timer_get_time();
  if (s_touch_pressed && s_touch_release_start_us != 0) {
    ESP_LOGI(TAG, "touch: bridged %" PRId64 "ms dropout",
             (now_us - s_touch_release_start_us) / 1000);
  } else if (!s_touch_pressed && s_touch_last_release_us != 0 &&
             now_us - s_touch_last_release_us < 300000) {
    ESP_LOGW(TAG,
             "touch: re-press %" PRId64 "ms after release (dropout %" PRId64
             "ms before release)",
             (now_us - s_touch_last_release_us) / 1000,
             s_touch_last_release_gap_us / 1000);
  }
#endif
  s_touch_pressed = true;
  s_touch_release_start_us = 0;
  s_touch_last_point.x = x;
  s_touch_last_point.y = y;
  data->state = LV_INDEV_STATE_PRESSED;
  data->point = s_touch_last_point;
}

/* The AXS15231B sometimes drops a read or reports zero points mid-press. */
static void touch_report_released(lv_indev_data_t *data) {
  data->point = s_touch_last_point;
  if (s_touch_pressed) {
    int64_t now_us = esp_timer_get_time();
    if (s_touch_release_start_us == 0) {
      s_touch_release_start_us = now_us;
    }
    if (now_us - s_touch_release_start_us < TOUCH_RELEASE_DEBOUNCE_MS * 1000) {
      data->state = LV_INDEV_STATE_PRESSED;
      return;
    }
    DISPLAY_PERF_SET(s_touch_releases, s_touch_releases + 1);
    DISPLAY_PERF_SET(s_touch_last_release_us, now_us);
    DISPLAY_PERF_SET(s_touch_last_release_gap_us,
                     now_us - s_touch_release_start_us);
  }
  s_touch_pressed = false;
  s_touch_release_start_us = 0;
  data->state = LV_INDEV_STATE_RELEASED;
}

static void touch_read(lv_indev_data_t *data) {
  uint8_t read_touchpad_cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0,
                                   0x0,  0x0e, 0x0,  0x0,  0x0};
  uint8_t buff[32] = {0};

  xSemaphoreTake(s_panel_chip_mutex, portMAX_DELAY);
  DISPLAY_PERF_SET(s_touch_in_i2c, true);
  esp_err_t touch_err = i2c_master_transmit_receive(
      touch_dev, read_touchpad_cmd, 11, buff, 32, pdMS_TO_TICKS(15));
  DISPLAY_PERF_SET(s_touch_in_i2c, false);
  xSemaphoreGive(s_panel_chip_mutex);
  if (touch_err != ESP_OK) {
    DISPLAY_PERF_SET(s_touch_i2c_errors, s_touch_i2c_errors + 1);
    touch_report_released(data);
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
    if (pointX > LCD_V_RES)
      pointX = LCD_V_RES;
    if (pointY > LCD_H_RES)
      pointY = LCD_H_RES;
    if (s_touch_pressed && s_touch_release_start_us != 0) {
      DISPLAY_PERF_SET(s_touch_gaps_bridged, s_touch_gaps_bridged + 1);
    }
    /* Map to native panel coordinates (172 x 640) */
    touch_report_pressed(data, pointY, LCD_V_RES - pointX);
  } else {
    touch_report_released(data);
  }
}

static void touch_input_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  touch_read(data);

  bool active = data->state == LV_INDEV_STATE_PRESSED ||
                lv_display_get_inactive_time(NULL) < TOUCH_POLL_IDLE_AFTER_MS;
  lv_timer_t *read_timer = lv_indev_get_read_timer(indev);
  if (read_timer) {
    lv_timer_set_period(read_timer,
                        active ? TOUCH_POLL_ACTIVE_MS : TOUCH_POLL_IDLE_MS);
  }
}

static uint32_t lvgl_tick_get_cb(void) {
  return (uint32_t)(esp_timer_get_time() / 1000);
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
      if (s_cpu_max_lock)
        esp_pm_lock_acquire(s_cpu_max_lock);
      task_delay_ms = lv_timer_handler();
      if (s_cpu_max_lock)
        esp_pm_lock_release(s_cpu_max_lock);
      DISPLAY_PERF_SET(s_lvgl_last_run_us, esp_timer_get_time());
      display_unlock();
    }
    if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
      task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    } else {
      /* LVGL animations need tighter handler pacing than idle operation.
       * Keep the 10 ms idle floor for power, but avoid adding a fixed 10 ms
       * gap between animation frames. */
      uint32_t min_delay_ms = lv_anim_count_running()
                                  ? LVGL_TASK_ANIM_MIN_DELAY_MS
                                  : LVGL_TASK_MIN_DELAY_MS;
      if (task_delay_ms < min_delay_ms) {
        task_delay_ms = min_delay_ms;
      }
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
      /* Keep PWM running through automatic light sleep. */
      .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE,
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

  /* Nothing is visible with the backlight off, so stop rendering and QSPI
   * transfers entirely; repaint the whole screen on wake. */
  bool off = percent == 0;
  if (s_disp && off != s_backlight_off) {
    lv_display_enable_invalidation(s_disp, !off);
    if (!off) {
      lv_obj_invalidate(lv_display_get_screen_active(s_disp));
    }
  }
  s_backlight_off = off;
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

    s_startup_version_lbl = lv_label_create(s_startup_scr);
    lv_obj_set_style_text_color(s_startup_version_lbl, lv_color_hex(0x71849A),
                                0);
    lv_obj_set_style_text_font(s_startup_version_lbl, &inter_16, 0);
    lv_obj_align(s_startup_version_lbl, LV_ALIGN_BOTTOM_LEFT, 8, -8);
  }

  lv_label_set_text(s_startup_title_lbl, title ? title : "Tomato32");
  lv_label_set_text(s_startup_subtitle_lbl,
                    subtitle ? subtitle : "Starting...");
  lv_label_set_text_fmt(s_startup_version_lbl, "v%s", TOMATO32_VERSION);
  lv_scr_load(s_startup_scr);
}

lv_display_t *display_init(void) {
  io_expander_init();
  lcd_reset();
  touch_init();

  s_frame_done_semaphore = xSemaphoreCreateBinary();
  assert(s_frame_done_semaphore);
  s_flush_queue = xQueueCreate(1, sizeof(flush_job_t));
  assert(s_flush_queue);
  s_panel_chip_mutex = xSemaphoreCreateMutex();
  assert(s_panel_chip_mutex);

  ESP_LOGI(TAG, "Initialize SPI bus");
  spi_bus_config_t buscfg = {
      .sclk_io_num = PIN_LCD_PCLK,
      .data0_io_num = PIN_LCD_DATA0,
      .data1_io_num = PIN_LCD_DATA1,
      .data2_io_num = PIN_LCD_DATA2,
      .data3_io_num = PIN_LCD_DATA3,
      .max_transfer_sz = DMA_BUFF_LEN,
      /* SPI hangs if its ISR runs on a different core than flush_task. */
      .isr_cpu_id = ESP_INTR_CPU_AFFINITY_1,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  ESP_LOGI(TAG, "Install panel IO");
  s_panel_io = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = PIN_LCD_CS,
      .dc_gpio_num = -1,
      .spi_mode = 3,
      .pclk_hz = TOMATO32_LCD_PCLK_HZ,
      .trans_queue_depth = 10,
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
  lv_tick_set_cb(lvgl_tick_get_cb);
  if (esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "lvgl", &s_cpu_max_lock) !=
      ESP_OK) {
    s_cpu_max_lock = NULL;
  }
  lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
#if TOMATO32_DISPLAY_USE_SWAPPED_RGB565
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
#endif
  lv_display_set_flush_cb(disp, example_lvgl_flush_cb);
  lv_display_set_flush_wait_cb(disp, example_lvgl_flush_wait_cb);

  uint8_t *buf1 = heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  uint8_t *buf2 = heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  trans_buf_1 = (uint16_t *)heap_caps_malloc(DMA_BUFF_LEN, MALLOC_CAP_DMA);
  trans_buf_2 = (uint16_t *)heap_caps_malloc(DMA_BUFF_LEN, MALLOC_CAP_DMA);
  assert(buf1 && buf2 && trans_buf_1 && trans_buf_2);
  xTaskCreatePinnedToCore(flush_task, "lcd_flush", FLUSH_TASK_STACK_SIZE, NULL,
                          FLUSH_TASK_PRIORITY, NULL, FLUSH_TASK_CORE);

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
  display_wait_flush_idle();

  lcd_bl_init();
  display_set_brightness(80);

  /* Touch input */
  lv_indev_t *touch_indev = lv_indev_create();
  lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch_indev, touch_input_read_cb);
  lv_indev_set_display(touch_indev, disp);

  s_disp = disp;
  lvgl_mux = xSemaphoreCreateMutex();
  assert(lvgl_mux);
  xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE,
                          NULL, LVGL_TASK_PRIORITY, NULL, 0);
#if TOMATO32_DISPLAY_PERF
  xTaskCreate(display_perf_report_task, "disp_perf", 4096, NULL, 1, NULL);
#endif

  return disp;
}
