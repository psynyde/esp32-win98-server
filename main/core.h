#ifndef CORE_H
#define CORE_H

#include "cJSON.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_netif_types.h"
#include "esp_partition.h"

#include <stdint.h>

#if SOC_TEMP_SENSOR_SUPPORTED
#  include "driver/temperature_sensor.h"
#endif

#define TAG "webserver"

typedef struct {
  char chip_model[128];
  char cpu_cores[16];
  char flash_size[16];
  char ip_address[16];
  char wifi_mac[18];
  char total_heap[16];
  char free_heap[32];
  char min_free_heap[32];
  uint32_t uptime;
  char temperature[16];
  cJSON *partitions;
} sysinfo_data_t;

typedef httpd_handle_t (*wifi_connected_cb_t)(void);

void led_turn_off_callback(void *arg);
void flash_led(void);
esp_err_t init_led(void);

#if SOC_TEMP_SENSOR_SUPPORTED
void initialize_temperature_sensor(void);
#endif

esp_err_t init_littlefs(void);

const char *get_chip_model_string(esp_chip_model_t model);
const char *get_partition_subtype_str(esp_partition_type_t type,
                                      esp_partition_subtype_t subtype);

esp_err_t get_sysinfo_data(sysinfo_data_t *data);
void free_sysinfo_data(sysinfo_data_t *data);

cJSON *load_guestbook_entries(void);
esp_err_t save_guestbook_entry(const char *name, const char *message);
cJSON *get_paginated_entries(cJSON *all_entries, int page, int *total_pages);

void register_wifi_callback(wifi_connected_cb_t callback);
esp_err_t init_wifi(void);
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);

#endif  // CORE_H
