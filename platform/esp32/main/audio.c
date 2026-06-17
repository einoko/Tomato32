#include "audio.h"
#include "codec_board.h"
#include "codec_init.h"
#include "display.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TAG "audio"
#define BELL_PCM_PATH "/spiffs/bell.pcm"
#define AUDIO_STREAM_CHUNK_SIZE 2048
#define AUDIO_TASK_STACK_SIZE (8 * 1024)
#define AUDIO_TASK_PRIORITY 5
#define AUDIO_TASK_CORE 1
#define BELL_SAMPLE_RATE 24000
#define BELL_CHANNELS 2
#define BELL_BITS_PER_SAMPLE 16

static esp_codec_dev_handle_t playback_dev = NULL;
static bool audio_ready = false;
static SemaphoreHandle_t audio_lock = NULL;
static TaskHandle_t playback_task_handle = NULL;
static uint8_t *s_bell_pcm_data = NULL;
static size_t s_bell_pcm_size = 0;
static uint8_t s_bell_volume_percent = 100;

static esp_codec_dev_sample_info_t bell_fs = {
    .sample_rate = BELL_SAMPLE_RATE,
    .channel = BELL_CHANNELS,
    .bits_per_sample = BELL_BITS_PER_SAMPLE,
};

void audio_set_bell_volume(uint8_t percent) {
  if (percent < 10) {
    percent = 10;
  }
  if (percent > 100) {
    percent = 100;
  }
  s_bell_volume_percent = percent;
}

static bool audio_load_bell_pcm_into_ram(void) {
  FILE *f = fopen(BELL_PCM_PATH, "rb");
  if (!f) {
    ESP_LOGW(TAG, "Bell preload: failed to open %s", BELL_PCM_PATH);
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    ESP_LOGW(TAG, "Bell preload: fseek end failed");
    return false;
  }

  long file_size = ftell(f);
  if (file_size <= 0) {
    fclose(f);
    ESP_LOGW(TAG, "Bell preload: invalid size %ld", file_size);
    return false;
  }

  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    ESP_LOGW(TAG, "Bell preload: fseek set failed");
    return false;
  }

  uint8_t *buf =
      heap_caps_malloc((size_t)file_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) {
    buf = heap_caps_malloc((size_t)file_size, MALLOC_CAP_8BIT);
  }
  if (!buf) {
    fclose(f);
    ESP_LOGW(TAG, "Bell preload: alloc failed (%ld bytes)", file_size);
    return false;
  }

  size_t read_size = fread(buf, 1, (size_t)file_size, f);
  fclose(f);

  if (read_size != (size_t)file_size) {
    heap_caps_free(buf);
    ESP_LOGW(TAG, "Bell preload: short read (%u/%u)", (unsigned)read_size,
             (unsigned)file_size);
    return false;
  }

  s_bell_pcm_data = buf;
  s_bell_pcm_size = (size_t)file_size;
  ESP_LOGI(TAG, "Bell preload: loaded %u bytes into RAM",
           (unsigned)s_bell_pcm_size);
  return true;
}

static void audio_shutdown_output_path(void) {
  int rc = esp_codec_dev_set_out_mute(playback_dev, true);
  if (rc != ESP_CODEC_DEV_OK) {
    ESP_LOGW(TAG, "Failed to mute output (%d)", rc);
  }

  rc = esp_codec_dev_close(playback_dev);
  if (rc != ESP_CODEC_DEV_OK) {
    ESP_LOGW(TAG, "Failed to close playback codec (%d)", rc);
  }

  if (!display_amp_disable()) {
    ESP_LOGW(TAG, "Failed to disable amp after playback");
  }
}

static void audio_playback_task(void *arg) {
  (void)arg;
  ESP_LOGI(TAG, "Playback task started");

  if (xSemaphoreTake(audio_lock, portMAX_DELAY) != pdTRUE) {
    playback_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }

  if (!display_amp_enable()) {
    ESP_LOGW(TAG, "Failed to enable amp before playback");
  }

  if (esp_codec_dev_open(playback_dev, &bell_fs) != ESP_CODEC_DEV_OK) {
    ESP_LOGE(TAG, "Failed to open playback codec");
    display_amp_disable();
    xSemaphoreGive(audio_lock);
    playback_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }

  int rc =
      esp_codec_dev_set_out_vol(playback_dev, (float)s_bell_volume_percent);
  if (rc != ESP_CODEC_DEV_OK) {
    ESP_LOGW(TAG, "Failed to set output volume (%d)", rc);
  }
  rc = esp_codec_dev_set_out_mute(playback_dev, false);
  if (rc != ESP_CODEC_DEV_OK) {
    ESP_LOGW(TAG, "Failed to unmute output (%d)", rc);
  }

  /* Give analog path a tiny settle window before first write. */
  vTaskDelay(pdMS_TO_TICKS(5));

  size_t total_written = 0;

  if (s_bell_pcm_data && s_bell_pcm_size > 0) {
    uint8_t *p = s_bell_pcm_data;
    size_t remain = s_bell_pcm_size;
    while (remain > 0) {
      size_t chunk =
          (remain > AUDIO_STREAM_CHUNK_SIZE) ? AUDIO_STREAM_CHUNK_SIZE : remain;
      int err = esp_codec_dev_write(playback_dev, p, chunk);
      if (err != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Codec write error %d after %d bytes (RAM)", err,
                 (int)total_written);
        break;
      }
      p += chunk;
      remain -= chunk;
      total_written += chunk;
    }

    ESP_LOGI(TAG, "Bell playback done (%d bytes written from RAM)",
             (int)total_written);
    audio_shutdown_output_path();
    xSemaphoreGive(audio_lock);
    playback_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }

  FILE *f = fopen(BELL_PCM_PATH, "rb");
  if (!f) {
    ESP_LOGE(TAG, "Failed to open %s", BELL_PCM_PATH);
    audio_shutdown_output_path();
    xSemaphoreGive(audio_lock);
    playback_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }
  ESP_LOGI(TAG, "Bell file opened OK");

  uint8_t buf[AUDIO_STREAM_CHUNK_SIZE];
  size_t bytes_read;

  while ((bytes_read = fread(buf, 1, AUDIO_STREAM_CHUNK_SIZE, f)) > 0) {
    int err = esp_codec_dev_write(playback_dev, buf, bytes_read);
    if (err != ESP_CODEC_DEV_OK) {
      ESP_LOGE(TAG, "Codec write error %d after %d bytes", err,
               (int)total_written);
      break;
    }
    total_written += bytes_read;
  }

  fclose(f);
  ESP_LOGI(TAG, "Bell playback done (%d bytes written)", (int)total_written);

  audio_shutdown_output_path();

  xSemaphoreGive(audio_lock);
  playback_task_handle = NULL;
  vTaskDelete(NULL);
}

bool audio_init(void) {
  if (audio_ready) {
    return true;
  }

  set_codec_board_type("S3_LCD_3_49");
  codec_init_cfg_t codec_cfg = {
      .in_mode = CODEC_I2S_MODE_TDM,
      .out_mode = CODEC_I2S_MODE_TDM,
      .in_use_tdm = false,
      .reuse_dev = false,
  };

  if (init_codec(&codec_cfg) != 0) {
    ESP_LOGE(TAG, "Codec init failed");
    return false;
  }

  playback_dev = get_playback_handle();
  if (!playback_dev) {
    ESP_LOGE(TAG, "No playback handle");
    return false;
  }

  if (!audio_lock) {
    audio_lock = xSemaphoreCreateMutex();
    if (!audio_lock) {
      ESP_LOGE(TAG, "Failed to create audio mutex");
      return false;
    }
  }

  esp_codec_set_disable_when_closed(playback_dev, true);

  if (!display_amp_disable()) {
    ESP_LOGW(TAG, "Failed to keep amp disabled after init");
  }

  (void)audio_load_bell_pcm_into_ram();

  audio_ready = true;
  ESP_LOGI(TAG, "Audio init OK");
  return true;
}

void audio_play_bell(void) {
  if (!audio_ready || !playback_dev) {
    ESP_LOGW(TAG, "Audio not ready");
    return;
  }

  if (playback_task_handle != NULL) {
    ESP_LOGW(TAG, "Bell playback already in progress");
    return;
  }

  ESP_LOGI(TAG, "Spawning bell playback task");
  /* Spawn playback in a background task so LVGL isn't blocked */
  if (xTaskCreatePinnedToCore(audio_playback_task, "audio_bell",
                              AUDIO_TASK_STACK_SIZE, NULL, AUDIO_TASK_PRIORITY,
                              &playback_task_handle,
                              AUDIO_TASK_CORE) != pdPASS) {
    playback_task_handle = NULL;
    ESP_LOGE(TAG, "Failed to create playback task");
  }
}
