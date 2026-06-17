#include "storage.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "storage";

bool storage_init(void) {
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = true,
  };

  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
    return false;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "SPIFFS: used %d / %d", used, total);
  }
  return true;
}
