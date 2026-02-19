#include "core.h"

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#if SOC_TEMP_SENSOR_SUPPORTED
static temperature_sensor_handle_t temp_sensor = NULL;
#endif

static esp_timer_handle_t turn_off_timer;
static wifi_connected_cb_t wifi_connected_callback = NULL;

void register_wifi_callback(wifi_connected_cb_t callback) {
  wifi_connected_callback = callback;
}

#if SOC_TEMP_SENSOR_SUPPORTED
void initialize_temperature_sensor(void) {
  ESP_LOGI(TAG, "Initializing temperature sensor");
  temperature_sensor_config_t temp_sensor_config =
      TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  esp_err_t ret = temperature_sensor_install(&temp_sensor_config, &temp_sensor);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install temperature sensor: %s",
             esp_err_to_name(ret));
    temp_sensor = NULL;
    return;
  }
  ret = temperature_sensor_enable(temp_sensor);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable temperature sensor: %s",
             esp_err_to_name(ret));
    temperature_sensor_uninstall(temp_sensor);
    temp_sensor = NULL;
  } else {
    ESP_LOGI(TAG, "Temperature sensor enabled");
  }
}

temperature_sensor_handle_t get_temp_sensor(void) {
  return temp_sensor;
}
#endif

void led_turn_off_callback(void *arg) {
  gpio_set_level(CONFIG_LED_GPIO, 0);
}

void flash_led(void) {
  gpio_set_level(CONFIG_LED_GPIO, 1);
  esp_timer_start_once(turn_off_timer, 1000 * 1000);
}

esp_err_t init_led(void) {
  gpio_reset_pin(CONFIG_LED_GPIO);
  gpio_set_direction(CONFIG_LED_GPIO, GPIO_MODE_OUTPUT);

  const esp_timer_create_args_t turn_off_timer_args = {
      .callback = &led_turn_off_callback, .name = "led-turn-off"};
  return esp_timer_create(&turn_off_timer_args, &turn_off_timer);
}

esp_err_t init_littlefs(void) {
  ESP_LOGI(TAG, "Initializing LittleFS");

  esp_vfs_littlefs_conf_t conf = {
      .base_path = "/littlefs",
      .partition_label = CONFIG_STORAGE_LABEL,
      .format_if_mount_failed = true,
      .dont_mount = false,
  };

  esp_err_t ret = esp_vfs_littlefs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find LittleFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
    }
    return ret;
  }

  size_t total = 0;
  size_t used = 0;
  ret = esp_littlefs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)",
             esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }
  return ESP_OK;
}

const char *get_chip_model_string(esp_chip_model_t model) {
  switch (model) {
  case CHIP_ESP32:
    return "Esp32";
  case CHIP_ESP32S2:
    return "Esp32-S2";
  case CHIP_ESP32S3:
    return "Esp32-S3";
  case CHIP_ESP32C3:
    return "Esp32-C3";
  case CHIP_ESP32C2:
    return "Esp32-C2";
  case CHIP_ESP32C6:
    return "Esp32-C6";
  case CHIP_ESP32H2:
    return "Esp32-H2";
  case CHIP_ESP32P4:
    return "Esp32-P4";
  case CHIP_ESP32C61:
    return "Esp32-C61";
  case CHIP_ESP32C5:
    return "Esp32-C5";
  case CHIP_ESP32H21:
    return "Esp32-H21";
  case CHIP_ESP32H4:
    return "Esp32-H4";
  case CHIP_POSIX_LINUX:
    return "Simulator";
  default:
    return "Unknown";
  }
}

const char *get_partition_subtype_str(esp_partition_type_t type,
                                      esp_partition_subtype_t subtype) {
  if (type == ESP_PARTITION_TYPE_APP) {
    switch (subtype) {
    case ESP_PARTITION_SUBTYPE_APP_FACTORY:
      return "factory";
    case ESP_PARTITION_SUBTYPE_APP_TEST:
      return "test";
    default:
      if (subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
          subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
        static char ota_str[8];
        snprintf(ota_str, sizeof(ota_str), "ota_%d",
                 subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
        return ota_str;
      }
      return "unknown_app";
    }
  } else if (type == ESP_PARTITION_TYPE_DATA) {
    switch (subtype) {
    case ESP_PARTITION_SUBTYPE_DATA_OTA:
      return "ota";
    case ESP_PARTITION_SUBTYPE_DATA_NVS:
      return "nvs";
    case ESP_PARTITION_SUBTYPE_DATA_PHY:
      return "phy";
    case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS:
      return "nvs_keys";
    case ESP_PARTITION_SUBTYPE_DATA_LITTLEFS:
      return "littlefs";
    default:
      return "unknown_data";
    }
  }
  return "unknown";
}

esp_err_t get_sysinfo_data(sysinfo_data_t *data) {
  if (!data)
    return ESP_FAIL;

  memset(data, 0, sizeof(sysinfo_data_t));

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  const char *model_str = get_chip_model_string(chip_info.model);
  uint32_t flash_size;
  esp_flash_get_size(NULL, &flash_size);
  uint32_t free_heap = esp_get_free_heap_size();
  uint32_t min_free_heap = esp_get_minimum_free_heap_size();
  uint32_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"),
                        &ip_info);
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);

  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%s (rev %d)", model_str,
           chip_info.revision);
  snprintf(data->chip_model, sizeof(data->chip_model), "%s", buffer);

  snprintf(buffer, sizeof(buffer), "%d", chip_info.cores);
  snprintf(data->cpu_cores, sizeof(data->cpu_cores), "%s", buffer);

  snprintf(buffer, sizeof(buffer), "%u MB",
           (unsigned int)(flash_size / (1024 * 1024)));
  snprintf(data->flash_size, sizeof(data->flash_size), "%s", buffer);

  snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&ip_info.ip));
  snprintf(data->ip_address, sizeof(data->ip_address), "%s", buffer);

  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(data->wifi_mac, sizeof(data->wifi_mac), "%s", buffer);

  snprintf(buffer, sizeof(buffer), "%.2f KB", total_heap / 1024.0);
  snprintf(data->total_heap, sizeof(data->total_heap), "%s", buffer);

  snprintf(buffer, sizeof(buffer), "%.2f KB (%.1f%%)", free_heap / 1024.0,
           (float)free_heap / total_heap * 100.0);
  snprintf(data->free_heap, sizeof(data->free_heap), "%s", buffer);

  snprintf(buffer, sizeof(buffer), "%.2f KB (since boot)",
           min_free_heap / 1024.0);
  snprintf(data->min_free_heap, sizeof(data->min_free_heap), "%s", buffer);

  data->uptime =
      (unsigned int)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);

  float temp_celsius = 0.0;
  bool temp_available = false;
#if SOC_TEMP_SENSOR_SUPPORTED
  if (temp_sensor != NULL &&
      temperature_sensor_get_celsius(temp_sensor, &temp_celsius) == ESP_OK) {
    temp_available = true;
    ESP_LOGI(TAG, "Current temperature: %.1f°C", temp_celsius);
  }
#endif
  if (temp_available) {
    snprintf(buffer, sizeof(buffer), "%.1f °C", temp_celsius);
  } else {
    snprintf(buffer, sizeof(buffer), "N/A");
  }
  snprintf(data->temperature, sizeof(data->temperature), "%s", buffer);

  data->partitions = cJSON_CreateArray();
  if (!data->partitions)
    return ESP_FAIL;

  esp_partition_iterator_t it = esp_partition_find(
      ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  for (; it != NULL; it = esp_partition_next(it)) {
    const esp_partition_t *p = esp_partition_get(it);
    if (p != NULL) {
      cJSON *part_obj = cJSON_CreateObject();
      cJSON_AddStringToObject(part_obj, "label", p->label);
      cJSON_AddStringToObject(
          part_obj, "type", p->type == ESP_PARTITION_TYPE_APP ? "app" : "data");
      cJSON_AddStringToObject(part_obj, "subtype",
                              get_partition_subtype_str(p->type, p->subtype));
      cJSON_AddNumberToObject(part_obj, "size", p->size);
      cJSON_AddBoolToObject(part_obj, "encrypted", p->encrypted);
      if (p->type == ESP_PARTITION_TYPE_DATA &&
          p->subtype == ESP_PARTITION_SUBTYPE_DATA_LITTLEFS) {
        size_t total_bytes;
        size_t used_bytes;
        if (esp_littlefs_info(p->label, &total_bytes, &used_bytes) == ESP_OK) {
          cJSON_AddNumberToObject(part_obj, "used", used_bytes);
        }
      }
      cJSON_AddItemToArray(data->partitions, part_obj);
    }
  }
  esp_partition_iterator_release(it);

  return ESP_OK;
}

void free_sysinfo_data(sysinfo_data_t *data) {
  if (data && data->partitions) {
    cJSON_Delete(data->partitions);
    data->partitions = NULL;
  }
}

cJSON *load_guestbook_entries(void) {
  cJSON *all_entries = NULL;
  FILE *f = fopen(CONFIG_GUESTBOOK_FILE, "r");
  if (f) {
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *file_data = malloc(fsize + 1);
    if (file_data) {
      fread(file_data, 1, fsize, f);
      file_data[fsize] = 0;
      all_entries = cJSON_Parse(file_data);
      free(file_data);
    }
    fclose(f);
  }

  if (!all_entries) {
    all_entries = cJSON_CreateArray();
  }

  return all_entries;
}

cJSON *get_paginated_entries(cJSON *all_entries, int page, int *total_pages) {
  int entries_per_page = 5;
  int total_entries = cJSON_GetArraySize(all_entries);

  *total_pages =
      (total_entries > 0) ? ((total_entries - 1) / entries_per_page) + 1 : 0;

  if (page < 0)
    page = 0;
  if (page >= *total_pages && *total_pages > 0)
    page = *total_pages - 1;

  cJSON *paginated_entries = cJSON_CreateArray();
  if (total_entries > 0) {
    int start_index = total_entries - 1 - (page * entries_per_page);
    int end_index = start_index - entries_per_page + 1;
    if (end_index < 0)
      end_index = 0;

    for (int i = start_index; i >= end_index; i--) {
      cJSON *entry = cJSON_GetArrayItem(all_entries, i);
      if (entry) {
        cJSON_AddItemToArray(paginated_entries, cJSON_Duplicate(entry, true));
      }
    }
  }

  return paginated_entries;
}

esp_err_t save_guestbook_entry(const char *name, const char *message) {
  cJSON *gb_array = load_guestbook_entries();

  cJSON *new_entry = cJSON_CreateObject();
  cJSON_AddStringToObject(new_entry, "n", name);
  cJSON_AddStringToObject(new_entry, "m", message);
  cJSON_AddItemToArray(gb_array, new_entry);

  int size = cJSON_GetArraySize(gb_array);
  if (size > CONFIG_MAX_GUESTBOOK_ENTRIES) {
    cJSON_DeleteItemFromArray(gb_array, 0);
  }

  char *out = cJSON_PrintUnformatted(gb_array);
  FILE *f = fopen(CONFIG_GUESTBOOK_FILE, "w");
  if (f) {
    fprintf(f, "%s", out);
    fclose(f);
  }
  free(out);
  cJSON_Delete(gb_array);

  return ESP_OK;
}

esp_err_t init_wifi(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler, NULL));

  wifi_config_t wifi_config = {
      .sta =
          {
                .ssid = CONFIG_WIFI_SSID,
                .password = CONFIG_WIFI_PASS,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  return ESP_OK;
}

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    if (wifi_connected_callback) {
      wifi_connected_callback();
    }
  }
}
