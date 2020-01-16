// Copyright (c) Janghun LEE. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#include "wifi_setting_server.h"

#define urlcmp(destination, source) strncmp(destination, source, strlen(source))
#define res_send_chunk(req, message) \
  httpd_resp_send_chunk(req, message, strlen(message))

#define SOFT_AP_IP_ADDRESS_1 192
#define SOFT_AP_IP_ADDRESS_2 168
#define SOFT_AP_IP_ADDRESS_3 0
#define SOFT_AP_IP_ADDRESS_4 4

#define SOFT_AP_GW_ADDRESS_1 192
#define SOFT_AP_GW_ADDRESS_2 168
#define SOFT_AP_GW_ADDRESS_3 0
#define SOFT_AP_GW_ADDRESS_4 2

#define SOFT_AP_NM_ADDRESS_1 255
#define SOFT_AP_NM_ADDRESS_2 255
#define SOFT_AP_NM_ADDRESS_3 255
#define SOFT_AP_NM_ADDRESS_4 0

#define SERVER_PORT 80
// #define HTTP_PUT HTTP_GET

#define DEFAULT_AP_SSID "ESP32 SoftAP"

static char ap_ssid[32] = DEFAULT_AP_SSID;
const static char* TAG = "wifi setting server";

httpd_handle_t server_handler = NULL;
const int WIFI_CONNECTED_BIT = BIT0;
EventGroupHandle_t wifi_event_group;

void start_server(void);
void stop_server(void);
static esp_err_t wifi_event_handler(void* user_parameter,
                                    system_event_t* event) {
  switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
      esp_wifi_connect();
      start_server();
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_CONNECTED");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP Got IP: %s\n",
               ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
      xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
      stop_server();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
      xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
      esp_wifi_connect();
      start_server();
      break;
    case SYSTEM_EVENT_STA_LOST_IP:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_LOST_IP");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STACONNECTED");
      esp_wifi_disconnect();
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STADISCONNECTED");
      esp_wifi_connect();
      break;
    default:
      break;
  }
  return ESP_OK;
}

esp_err_t index_get_handler(httpd_req_t* req) {
  esp_wifi_disconnect();
  esp_wifi_scan_stop();

  res_send_chunk(req, "<!DOCTYPE html><html><head>");
  res_send_chunk(
      req,
      "<meta name='viewport' content='width=device-width, initial-scale=1' />");
  res_send_chunk(
      req,
      "<meta charset='UTF-8' /><link rel='shortcut icon' href='/favicon.ico' "
      "type='image/x-icon'/>");
  res_send_chunk(req,
                 "<link rel='icon' href='/favicon.ico' type='image/x-icon'/>");
  res_send_chunk(req,
                 "<link rel='stylesheet' type='text/css' href='/style.css'>");
  res_send_chunk(
      req, "<script type='text/javascript' src='/jquery.min.js'></script>");
  res_send_chunk(req, "<title>WiFi-Station</title>");
  res_send_chunk(req, "<script>function checkSSID() { ");
  res_send_chunk(req, "var formEl = document.forms.wifiForm;");
  res_send_chunk(req, "var formData = new FormData(formEl);");
  res_send_chunk(req, "var name = formData.get('ssid');");
  res_send_chunk(req, "if (name === 'Select your Wi-Fi') {");
  res_send_chunk(req, "alert('Please select your Wi-Fi.'); ");
  res_send_chunk(req, "return false; }");
  res_send_chunk(req, "}</script>");
  res_send_chunk(req, "<script>$(document).ready(function(){");
  res_send_chunk(req, "$('#wifiForm').submit(function(e) {");
  res_send_chunk(req, "const form = $(this);");
  res_send_chunk(req,
                 "const data = { ssid :$('#wifiForm [name=\"ssid\"]').val(),");
  res_send_chunk(req, "password: $('#wifiForm [name=\"password\"]').val()};");
  res_send_chunk(req, "const url = form.attr('action');");
  res_send_chunk(req, "$.ajax({");
  res_send_chunk(req, "type: 'POST',");
  res_send_chunk(req, "url: url,");
  res_send_chunk(req, "data: JSON.stringify(data),");
  res_send_chunk(req, "dataType: 'json'");
  res_send_chunk(req, "});");
  res_send_chunk(req, "});");
  res_send_chunk(req, "}); </script>");

  res_send_chunk(req, "</head><body><div class='container'>");
  res_send_chunk(req, "<div class='title'>");
  res_send_chunk(req, "<h1 style='text-align: center;'>WiFi-Station</h1>");
  res_send_chunk(
      req,
      "<button class='wifi-reload' onclick='window.location.reload(); return "
      "false;'></button>");
  res_send_chunk(req, "</div>");
  res_send_chunk(req,
                 "<form method='get' enctype=''action='/save' id='wifiForm'>");
  res_send_chunk(req, "<div class='field-group'>");
  res_send_chunk(req, "<select name='ssid'>");
  res_send_chunk(req, "<option>Select your Wi-Fi</option>");

  wifi_scan_config_t scan_config = {.scan_type = WIFI_SCAN_TYPE_ACTIVE};
  esp_wifi_scan_start(&scan_config, true);
  uint16_t number = 0;
  esp_wifi_scan_get_ap_num(&number);
  wifi_ap_record_t* ap_info =
      (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * number);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  for (int i = 0; (i < number); i++) {
    ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
    res_send_chunk(req, "<option>");
    res_send_chunk(req, (char*)ap_info[i].ssid);
    res_send_chunk(req, "</option>");
  }
  free(ap_info);
  res_send_chunk(req,
                 "<input name='password' type='text' length='64' "
                 "onkeyup='inputKeyUp(event)' placeholder='Password' "
                 "autocomplete='off'>");
  res_send_chunk(req, "</div><div class='button-container'>");
  res_send_chunk(
      req, "<button type='submit' onclick='return checkSSID()'>Save</button>");
  res_send_chunk(req, "</div></form></div></body></html>");
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
esp_err_t favicon_get_handler(httpd_req_t* req) {
  extern const unsigned char favicon_ico_start[] asm(
      "_binary_favicon_ico_start");
  extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
  const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char*)favicon_ico_start, favicon_ico_size);
  return ESP_OK;
}
esp_err_t jquery_get_handler(httpd_req_t* req) {
  extern const unsigned char jquery_js_start[] asm(
      "_binary_jquery_min_js_start");
  extern const unsigned char jquery_js_end[] asm("_binary_jquery_min_js_end");
  const size_t jquery_js_size = (jquery_js_end - jquery_js_start);
  httpd_resp_set_type(req, "text/javascript");
  httpd_resp_send(req, (const char*)jquery_js_start, jquery_js_size);
  return ESP_OK;
}

esp_err_t style_get_handler(httpd_req_t* req) {
  extern const unsigned char style_css_start[] asm("_binary_style_css_start");
  extern const unsigned char style_css_end[] asm("_binary_style_css_end");
  const size_t style_css_size = (style_css_end - style_css_start);
  httpd_resp_set_type(req, "text/css");
  httpd_resp_send(req, (const char*)style_css_start, style_css_size);
  return ESP_OK;
}

esp_err_t refresh_get_handler(httpd_req_t* req) {
  extern const unsigned char refresh_png_start[] asm(
      "_binary_refresh_png_start");
  extern const unsigned char refresh_png_end[] asm("_binary_refresh_png_end");
  const size_t refresh_png_size = (refresh_png_end - refresh_png_start);
  httpd_resp_set_type(req, "image/png");
  httpd_resp_send(req, (const char*)refresh_png_start, refresh_png_size);
  return ESP_OK;
}
esp_err_t save_get_handler(httpd_req_t* req) {
  extern const unsigned char save_html_start[] asm("_binary_save_html_start");
  extern const unsigned char save_html_end[] asm("_binary_save_html_end");
  const size_t save_html_size = (save_html_end - save_html_start);
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, (const char*)save_html_start, save_html_size);
  return ESP_OK;
}
esp_err_t save_post_handler(httpd_req_t* req) {
  size_t content_size = req->content_len;
  char* body = (char*)malloc(sizeof(char) * (content_size + 1));
  memset(body, 0, content_size + 1);
  int ret = httpd_req_recv(req, body, content_size);
  if (ret < 0) {
    ESP_LOGE(TAG, "Failed to receivce data.");
    return ESP_FAIL;
  }
  printf("body: %s \n", body);
  JSON_Value* root_value = json_parse_string(body);
  JSON_Object* root_object = json_value_get_object(root_value);
  char* ssid = (char*)json_object_get_string(root_object, "ssid");
  char* password = (char*)json_object_get_string(root_object, "password");
  printf("ssid %s\n", ssid);
  printf("pw %s\n", password);
  wifi_config_t wifi_config;
  memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));
  memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
  strncpy((char*)wifi_config.sta.ssid, ssid, strlen(ssid));
  strncpy((char*)wifi_config.sta.password, password, strlen(password));
  ESP_LOGI("WIFI", "Setting WiFi configuration SSID %s...",
           wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  esp_wifi_connect();
  free(body);
  json_value_free(root_value);
  return ESP_OK;
}

static esp_err_t get_handlers(httpd_req_t* req) {
  if (urlcmp(req->uri, "/favicon.ico") == 0) {
    return favicon_get_handler(req);
  }
  if (urlcmp(req->uri, "/jquery.min.js") == 0) {
    return jquery_get_handler(req);
  }
  if (urlcmp(req->uri, "/save") == 0) {
    return save_get_handler(req);
  }
  if (urlcmp(req->uri, "/style.css") == 0) {
    return style_get_handler(req);
  };
  if (urlcmp(req->uri, "/refresh.png") == 0) {
    return refresh_get_handler(req);
  };
  httpd_resp_set_status(req, "307 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);  // Response body can be empty
  return ESP_OK;
}

void start_server(void) {
  if (server_handler != NULL) return;
  ESP_LOGI(TAG, "start http server");
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  httpd_uri_t index_url = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = index_get_handler,
      .user_ctx = NULL,
  };

  httpd_uri_t get_uri = {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = get_handlers,
      .user_ctx = NULL,
  };

  httpd_uri_t post_uri = {
      .uri = "/save",
      .method = HTTP_POST,
      .handler = save_post_handler,
      .user_ctx = NULL,
  };

  httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
  server_config.uri_match_fn = httpd_uri_match_wildcard;
  server_config.server_port = SERVER_PORT;

  if (httpd_start(&server_handler, &server_config) == ESP_OK) {
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handler, &index_url));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handler, &get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handler, &post_uri));
  }
}

void stop_server(void) {
  ESP_LOGI(TAG, "stop http server");
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  if (server_handler != NULL) {
    ESP_ERROR_CHECK(httpd_stop(server_handler));
    server_handler = NULL;
  }
}

void start_wifi_setting_server() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  tcpip_adapter_init();
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));

  wifi_init_config_t wifiConfiguration = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifiConfiguration));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

  tcpip_adapter_ip_info_t ipAddressInfo;
  memset(&ipAddressInfo, 0, sizeof(ipAddressInfo));
  IP4_ADDR(&ipAddressInfo.gw, SOFT_AP_GW_ADDRESS_1, SOFT_AP_GW_ADDRESS_2,
           SOFT_AP_GW_ADDRESS_3, SOFT_AP_GW_ADDRESS_4);
  IP4_ADDR(&ipAddressInfo.ip, SOFT_AP_IP_ADDRESS_1, SOFT_AP_IP_ADDRESS_2,
           SOFT_AP_IP_ADDRESS_3, SOFT_AP_IP_ADDRESS_4);
  IP4_ADDR(&ipAddressInfo.netmask, SOFT_AP_NM_ADDRESS_1, SOFT_AP_NM_ADDRESS_2,
           SOFT_AP_NM_ADDRESS_3, SOFT_AP_NM_ADDRESS_4);
  ESP_ERROR_CHECK(
      tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ipAddressInfo));
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
  wifi_config_t ap_config = {
      .ap =
          {
              .ssid_len = 0,
              //.channel = default,
              .authmode = WIFI_AUTH_OPEN,
              .ssid_hidden = 0,
              .max_connection = 1,
              .beacon_interval = 150,
          },
  };
  strncpy((char*)ap_config.ap.ssid, ap_ssid, strlen(ap_ssid));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
  ESP_ERROR_CHECK(esp_wifi_set_protocol(
      ESP_IF_WIFI_STA,
      WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
  ESP_ERROR_CHECK(esp_wifi_start());
}

void wait_connect_wifi() {
  xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true,
                      portMAX_DELAY);
}

esp_err_t set_ap_ssid(char* ssid) {
  size_t length = strlen(ssid);
  if (length > 32) {
    ESP_LOGE(TAG, "ap ssid is too long.");
    return ESP_FAIL;
  }

  memset(ap_ssid, 0, strlen(ap_ssid));
  strncpy(ap_ssid, ssid, length);
  return ESP_OK;
}