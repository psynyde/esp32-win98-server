#include "server.h"

#include "core.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t W95FA_WOFF2_START[] asm("_binary_w95fa_woff2_start");
extern const uint8_t W95FA_WOFF2_END[] asm("_binary_w95fa_woff2_end");
extern const uint8_t NIX_GIF_START[] asm("_binary_built_with_nix_gif_start");
extern const uint8_t NIX_GIF_END[] asm("_binary_built_with_nix_gif_end");
extern const uint8_t NEOVIM_PNG_START[] asm("_binary_neovim_png_start");
extern const uint8_t NEOVIM_PNG_END[] asm("_binary_neovim_png_end");
extern const uint8_t UBLOCK_PNG_START[] asm("_binary_ublockorigin_png_start");
extern const uint8_t UBLOCK_PNG_END[] asm("_binary_ublockorigin_png_end");
extern const uint8_t NETSCAPE_GIF_START[] asm("_binary_netscape_gif_start");
extern const uint8_t NETSCAPE_GIF_END[] asm("_binary_netscape_gif_end");
extern const uint8_t ABOUT_HTML_START[] asm("_binary_about_html_start");
extern const uint8_t ABOUT_HTML_END[] asm("_binary_about_html_end");
extern const uint8_t SYSINFO_HTML_START[] asm("_binary_sysinfo_html_start");
extern const uint8_t SYSINFO_HTML_END[] asm("_binary_sysinfo_html_end");
extern const uint8_t GUESTBOOK_HTML_START[] asm("_binary_guestbook_html_start");
extern const uint8_t GUESTBOOK_HTML_END[] asm("_binary_guestbook_html_end");

static esp_err_t font_download_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "font/woff2");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");

  const size_t font_len = W95FA_WOFF2_END - W95FA_WOFF2_START;
  httpd_resp_send(req, (const char *)W95FA_WOFF2_START, font_len);
  return ESP_OK;
}

static esp_err_t nix_gif_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "image/gif");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
  const size_t len = NIX_GIF_END - NIX_GIF_START;
  httpd_resp_send(req, (const char *)NIX_GIF_START, len);
  return ESP_OK;
}

static esp_err_t neovim_png_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "image/png");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
  const size_t len = NEOVIM_PNG_END - NEOVIM_PNG_START;
  httpd_resp_send(req, (const char *)NEOVIM_PNG_START, len);
  return ESP_OK;
}

static esp_err_t ublock_png_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "image/png");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
  const size_t len = UBLOCK_PNG_END - UBLOCK_PNG_START;
  httpd_resp_send(req, (const char *)UBLOCK_PNG_START, len);
  return ESP_OK;
}

static esp_err_t netscape_gif_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "image/gif");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
  const size_t len = NETSCAPE_GIF_END - NETSCAPE_GIF_START;
  httpd_resp_send(req, (const char *)NETSCAPE_GIF_START, len);
  return ESP_OK;
}

static esp_err_t about_page_handler(httpd_req_t *req) {
  flash_led();
  httpd_resp_set_type(req, "text/html");
  const size_t len = ABOUT_HTML_END - ABOUT_HTML_START;
  httpd_resp_send(req, (const char *)ABOUT_HTML_START, len);
  return ESP_OK;
}

static esp_err_t sysinfo_page_handler(httpd_req_t *req) {
  flash_led();
  httpd_resp_set_type(req, "text/html");
  const size_t len = SYSINFO_HTML_END - SYSINFO_HTML_START;
  httpd_resp_send(req, (const char *)SYSINFO_HTML_START, len);
  return ESP_OK;
}

static esp_err_t guestbook_page_handler(httpd_req_t *req) {
  flash_led();
  httpd_resp_set_type(req, "text/html");
  const size_t len = GUESTBOOK_HTML_END - GUESTBOOK_HTML_START;
  httpd_resp_send(req, (const char *)GUESTBOOK_HTML_START, len);
  return ESP_OK;
}

static esp_err_t sysinfo_api_handler(httpd_req_t *req) {
  sysinfo_data_t data;
  if (get_sysinfo_data(&data) != ESP_OK) {
    return ESP_FAIL;
  }

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    free_sysinfo_data(&data);
    return ESP_FAIL;
  }

  cJSON_AddStringToObject(root, "chip_model", data.chip_model);
  cJSON_AddStringToObject(root, "cpu_cores", data.cpu_cores);
  cJSON_AddStringToObject(root, "flash_size", data.flash_size);
  cJSON_AddStringToObject(root, "ip_address", data.ip_address);
  cJSON_AddStringToObject(root, "wifi_mac", data.wifi_mac);
  cJSON_AddStringToObject(root, "total_heap", data.total_heap);
  cJSON_AddStringToObject(root, "free_heap", data.free_heap);
  cJSON_AddStringToObject(root, "min_free_heap", data.min_free_heap);
  cJSON_AddNumberToObject(root, "uptime", data.uptime);
  cJSON_AddStringToObject(root, "temperature", data.temperature);

  if (data.partitions) {
    cJSON_AddItemToObject(root, "partitions", data.partitions);
  }

  char *json_str = cJSON_Print(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, strlen(json_str));
  free(json_str);
  cJSON_Delete(root);

  return ESP_OK;
}

static esp_err_t guestbook_get_handler(httpd_req_t *req) {
  char query_buf[64];
  int page = 0;
  if (httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf)) ==
      ESP_OK) {
    char param_buf[10];
    if (httpd_query_key_value(query_buf, "page", param_buf,
                              sizeof(param_buf)) == ESP_OK) {
      page = atoi(param_buf);
    }
  }

  cJSON *all_entries = load_guestbook_entries();
  if (!all_entries) {
    all_entries = cJSON_CreateArray();
  }

  int total_pages = 0;
  cJSON *paginated_entries =
      get_paginated_entries(all_entries, page, &total_pages);

  cJSON *response_json = cJSON_CreateObject();
  cJSON_AddItemToObject(response_json, "entries", paginated_entries);
  cJSON_AddNumberToObject(response_json, "page", page);
  cJSON_AddNumberToObject(response_json, "totalPages", total_pages);

  char *response_str = cJSON_PrintUnformatted(response_json);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response_str, strlen(response_str));
  free(response_str);

  cJSON_Delete(all_entries);
  cJSON_Delete(response_json);

  return ESP_OK;
}

static esp_err_t guestbook_post_handler(httpd_req_t *req) {
  char buf[200];
  int ret;
  int remaining = req->content_len;
  if (remaining >= sizeof(buf)) {
    remaining = sizeof(buf) - 1;
  }
  if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {
    return ESP_FAIL;
  }
  buf[ret] = '\0';

  cJSON *root = cJSON_Parse(buf);
  if (!root) {
    return ESP_FAIL;
  }
  cJSON *name = cJSON_GetObjectItem(root, "name");
  cJSON *msg = cJSON_GetObjectItem(root, "message");

  if (cJSON_IsString(name) && cJSON_IsString(msg)) {
    save_guestbook_entry(name->valuestring, msg->valuestring);
    httpd_resp_send(req, "OK", 2);
  } else {
    httpd_resp_send_500(req);
  }
  cJSON_Delete(root);
  return ESP_OK;
}

static httpd_handle_t server = NULL;

httpd_handle_t start_webserver(void) {
  if (server) {
    return server;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_uri_handlers = 15;
  config.stack_size = 8192;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t about_uri = {
        .uri = "/", .method = HTTP_GET, .handler = about_page_handler};
    httpd_register_uri_handler(server, &about_uri);

    httpd_uri_t sys_uri = {
        .uri = "/sysinfo", .method = HTTP_GET, .handler = sysinfo_page_handler};
    httpd_register_uri_handler(server, &sys_uri);

    httpd_uri_t gb_uri = {.uri = "/guestbook",
                          .method = HTTP_GET,
                          .handler = guestbook_page_handler};
    httpd_register_uri_handler(server, &gb_uri);

    httpd_uri_t font_uri = {.uri = "/w95fa.woff2",
                            .method = HTTP_GET,
                            .handler = font_download_handler};
    httpd_register_uri_handler(server, &font_uri);

    httpd_uri_t nix_gif_uri = {.uri = "/built_with_nix.gif",
                               .method = HTTP_GET,
                               .handler = nix_gif_handler};
    httpd_register_uri_handler(server, &nix_gif_uri);
    httpd_uri_t neovim_png_uri = {.uri = "/neovim.png",
                                  .method = HTTP_GET,
                                  .handler = neovim_png_handler};
    httpd_register_uri_handler(server, &neovim_png_uri);
    httpd_uri_t ublock_png_uri = {.uri = "/ublockorigin.png",
                                  .method = HTTP_GET,
                                  .handler = ublock_png_handler};
    httpd_register_uri_handler(server, &ublock_png_uri);
    httpd_uri_t netscape_gif_uri = {.uri = "/netscape.gif",
                                    .method = HTTP_GET,
                                    .handler = netscape_gif_handler};
    httpd_register_uri_handler(server, &netscape_gif_uri);

    httpd_uri_t api_sys = {.uri = "/api/sysinfo",
                           .method = HTTP_GET,
                           .handler = sysinfo_api_handler};
    httpd_register_uri_handler(server, &api_sys);

    httpd_uri_t api_gb_get = {.uri = "/api/guestbook",
                              .method = HTTP_GET,
                              .handler = guestbook_get_handler};
    httpd_register_uri_handler(server, &api_gb_get);

    httpd_uri_t api_gb_post = {.uri = "/api/sign",
                               .method = HTTP_POST,
                               .handler = guestbook_post_handler};
    httpd_register_uri_handler(server, &api_gb_post);

    return server;
  }
  return NULL;
}
