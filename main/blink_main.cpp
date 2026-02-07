/**
 * ESP32-C3 Supermini - WiFi + Nhiet do cac thanh pho
 * - Ket noi WiFi
 * - Web UI: mo http://<IP-cua-ESP32>/ tren trinh duyet (cung WiFi)
 * - Serial: nhap ten thanh pho + Enter
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "linenoise/linenoise.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "wifi_config.h"
#include "esp_spiffs.h"
#include <sys/time.h>

#define LED_GPIO       GPIO_NUM_8
#define LOG_PATH       "/spiffs/wifi_log.txt"
#define WIFI_CONNECTED BIT0
#define WIFI_FAILED    BIT1
#define WIFI_TIMEOUT_MS 20000  // 20s timeout
#define HTTP_BUF       1024
#define WEATHER_MS     30000   // 30 giay cap nhat

static const char *TAG = "main";
static EventGroupHandle_t wifi_event_group;
static int wifi_retry = 0;

static void write_wifi_log(const char *reason, int retry_count)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = true
    };
    if (esp_vfs_spiffs_register(&conf) != ESP_OK) return;

    FILE *f = fopen(LOG_PATH, "a");
    if (f) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm t;
        time_t now = tv.tv_sec;
        localtime_r(&now, &t);
        fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] WiFi FAIL: %s (retry=%d)\n",
                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                t.tm_hour, t.tm_min, t.tm_sec, reason, retry_count);
        fclose(f);
        printf("[Log] Ghi vao %s\n", LOG_PATH);
    }
    esp_vfs_spiffs_unregister("storage");
}

static const char* wifi_reason_str(uint8_t r) {
    switch (r) {
        case 2:  return "Auth expired (sai mat khau?)";
        case 15: return "Handshake timeout (sai mat khau?)";
        case 201: return "No AP (khong tim thay WiFi)";
        case 202: return "Auth fail (sai mat khau)";
        default: return "Unknown";
    }
}

// HCM City: 10.8231, 106.6297
#define HCM_LAT "10.8231"
#define HCM_LON "106.6297"

// Cac thanh pho lon tren the gioi
typedef struct { const char *name; const char *lat; const char *lon; } city_t;
static const city_t CITIES[] = {
    {"HCM City",   "10.8231",  "106.6297"},
    {"Tokyo",      "35.6762",  "139.6503"},
    {"London",     "51.5074",  "-0.1278"},
    {"New York",   "40.7128",  "-74.0060"},
    {"Paris",      "48.8566",  "2.3522"},
    {"Sydney",     "-33.8688", "151.2093"},
    {"Dubai",      "25.2048",  "55.2708"},
    {"Singapore",  "1.3521",   "103.8198"},
};
#define NUM_CITIES (sizeof(CITIES) / sizeof(CITIES[0]))

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    static int offset = 0;
    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        offset = 0;
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->user_data && evt->data_len > 0) {
            int copy_len = evt->data_len;
            if (offset + copy_len >= HTTP_BUF) copy_len = HTTP_BUF - offset - 1;
            if (copy_len > 0) {
                memcpy((char *)evt->user_data + offset, evt->data, copy_len);
                offset += copy_len;
                ((char *)evt->user_data)[offset] = '\0';
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
    case HTTP_EVENT_DISCONNECTED:
        offset = 0;
        break;
    default:
        break;
    }
    return ESP_OK;
}

static float fetch_temp(const char *lat, const char *lon)
{
    char url[200];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m",
        lat, lon);

    char response[HTTP_BUF] = {0};

    esp_http_client_config_t config = {0};
    config.url = url;
    config.event_handler = http_event_handler;
    config.user_data = response;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 10000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return -999.0f;

    esp_err_t err = esp_http_client_perform(client);
    float temp = -999.0f;

    if (err == ESP_OK && esp_http_client_get_status_code(client) == 200) {
        cJSON *root = cJSON_Parse(response);
        if (root) {
            cJSON *current = cJSON_GetObjectItem(root, "current");
            if (current) {
                cJSON *t = cJSON_GetObjectItem(current, "temperature_2m");
                if (cJSON_IsNumber(t)) temp = (float)t->valuedouble;
            }
            cJSON_Delete(root);
        }
    }

    esp_http_client_cleanup(client);
    return temp;
}

static float fetch_hcm_temperature(void) { return fetch_temp(HCM_LAT, HCM_LON); }

// Trang HTML giao dien web
static const char* HTML_PAGE =
"<!DOCTYPE html><html><head><meta charset=UTF-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
"<title>Nhiet do</title>"
"<style>"
"*{box-sizing:border-box}body{margin:0;font-family:system-ui,sans-serif;background:linear-gradient(135deg,#1a1a2e,#16213e);min-height:100vh;display:flex;align-items:center;justify-content:center;color:#eee}"
".card{background:rgba(255,255,255,0.08);border-radius:16px;padding:32px;box-shadow:0 8px 32px rgba(0,0,0,0.3);text-align:center;max-width:380px}"
"h1{margin:0 0 24px;font-size:1.5rem;color:#4fc3f7}"
"select{width:100%;padding:12px 16px;font-size:1rem;border-radius:8px;border:1px solid #4fc3f7;background:#0d1117;color:#eee;margin-bottom:16px}"
"button{width:100%;padding:14px;font-size:1rem;border:none;border-radius:8px;background:#4fc3f7;color:#0d1117;font-weight:600;cursor:pointer}"
"button:hover{background:#29b6f6}"
"button:disabled{opacity:0.6;cursor:not-allowed}"
"#result{margin-top:24px;min-height:48px;font-size:1.8rem;font-weight:700}"
".temp{color:#4fc3f7}"
".err{color:#ef5350}"
".ip{font-size:0.75rem;color:#888;margin-top:16px}"
"</style></head><body>"
"<div class=card>"
"<h1>Nhiet do thanh pho</h1>"
"<select id=city>"
"<option value=0>HCM City</option><option value=1>Tokyo</option><option value=2>London</option>"
"<option value=3>New York</option><option value=4>Paris</option><option value=5>Sydney</option>"
"<option value=6>Dubai</option><option value=7>Singapore</option>"
"</select>"
"<button id=btn onclick=fetchTemp()>Tra cuu</button>"
"<div id=result></div>"
"<div class=ip>Mo http://&lt;IP&gt; tren cung WiFi</div>"
"</div>"
"<script>"
"async function fetchTemp(){"
"var b=document.getElementById('btn');b.disabled=true;document.getElementById('result').innerHTML='...';"
"try{"
"var r=await fetch('/temp?city='+document.getElementById('city').value);"
"var j=await r.json();"
"var el=document.getElementById('result');"
"if(j.ok)el.innerHTML='<span class=temp>'+j.city+': '+j.temp+' &deg;C</span>';"
"else el.innerHTML='<span class=err>'+j.err+'</span>';"
"}catch(e){document.getElementById('result').innerHTML='<span class=err>Loi ket noi</span>';}"
"b.disabled=false;"
"}"
"</script></body></html>";

static esp_err_t html_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, HTML_PAGE, strlen(HTML_PAGE));
}

static esp_err_t temp_api_handler(httpd_req_t *req)
{
    char buf[32];
    size_t len = httpd_req_get_url_query_len(req);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) != ESP_OK) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", false);
        cJSON_AddStringToObject(err, "err", "Thieu tham so");
        char *out = cJSON_PrintUnformatted(err);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, out ?: "{}");
        free(out);
        cJSON_Delete(err);
        return ESP_OK;
    }

    char city_val[8] = {0};
    if (httpd_query_key_value(buf, "city", city_val, sizeof(city_val)) != ESP_OK) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", false);
        cJSON_AddStringToObject(err, "err", "Thieu city");
        char *out = cJSON_PrintUnformatted(err);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, out ?: "{}");
        free(out);
        cJSON_Delete(err);
        return ESP_OK;
    }

    int idx = atoi(city_val);
    if (idx < 0 || idx >= (int)NUM_CITIES) idx = 0;

    float temp = fetch_temp(CITIES[idx].lat, CITIES[idx].lon);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "ok", temp > -900.0f);
    cJSON_AddStringToObject(j, "city", CITIES[idx].name);
    if (temp > -900.0f) {
        cJSON_AddNumberToObject(j, "temp", temp);
    } else {
        cJSON_AddStringToObject(j, "err", "Loi lay du lieu");
    }
    char *out = cJSON_PrintUnformatted(j);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out ?: "{}");
    free(out);
    cJSON_Delete(j);
    return ESP_OK;
}

static void start_web_server(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t html = { .uri = "/", .method = HTTP_GET, .handler = html_handler };
        httpd_uri_t temp = { .uri = "/temp", .method = HTTP_GET, .handler = temp_api_handler };
        httpd_register_uri_handler(server, &html);
        httpd_register_uri_handler(server, &temp);
        printf("[Web] Server: http://<IP-cua-ESP32>/\n");
    }
}

static void wifi_scan_first(void)
{
    wifi_scan_config_t scan_ref = {0};
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_ref, true));

    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    wifi_ap_record_t *ap = (wifi_ap_record_t *)malloc(n * sizeof(wifi_ap_record_t));
    if (ap && n > 0) {
        esp_wifi_scan_get_ap_records(&n, ap);
        printf("--- Tim thay %d WiFi ---\n", n);
        for (int i = 0; i < (int)n && i < 5; i++) {
            printf("  [%d] %-32s RSSI:%d\n", i + 1, (char *)ap[i].ssid, ap[i].rssi);
        }
        int found = 0;
        for (int i = 0; i < (int)n; i++) {
            if (strcmp((char *)ap[i].ssid, WIFI_SSID) == 0) {
                printf("  => %s CO trong danh sach (RSSI:%d)\n", WIFI_SSID, ap[i].rssi);
                found = 1;
                break;
            }
        }
        if (!found) printf("  => %s KHONG tim thay! Kiem tra SSID hoac di chuyen gan router.\n", WIFI_SSID);
        free(ap);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *evt = (wifi_event_sta_disconnected_t *)event_data;
        wifi_retry++;
        printf("[WiFi] Disconnect #%d - reason %d (%s)\n", wifi_retry, evt->reason, wifi_reason_str(evt->reason));

        if (wifi_retry > 10) {
            printf("[WiFi] FAILED - Kiem tra SSID/mat khau. Restart sau 30s...\n");
            xEventGroupSetBits(wifi_event_group, WIFI_FAILED);
        } else {
            vTaskDelay(pdMS_TO_TICKS(2000));  /* 2s giua cac lan thu - tranh flood router */
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
        wifi_retry = 0;  /* Reset khi connect thanh cong - tranh tich luy retry */
        printf("[WiFi] OK! IP: " IPSTR "\n", IP2STR(&evt->ip_info.ip));
        printf("[Web] Mo http://" IPSTR "/ tren trinh duyet\n", IP2STR(&evt->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED);
    }
}

static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t h1, h2;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL, &h1));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL, &h2));

    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.sta.ssid, WIFI_SSID, sizeof(wcfg.sta.ssid) - 1);
    strncpy((char *)wcfg.sta.password, WIFI_PASSWORD, sizeof(wcfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("Scan WiFi...\n");
    wifi_scan_first();
    printf("Connect to %s...\n", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_connect());
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED | WIFI_FAILED,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(WIFI_TIMEOUT_MS));

    if (bits & WIFI_FAILED) {
        write_wifi_log("Qua nhieu lan that bai", wifi_retry);
        printf("[WiFi] Qua nhieu lan that bai. Restart sau 10s...\n");
        vTaskDelay(pdMS_TO_TICKS(10000));  /* Tranh restart loop lien tuc */
        esp_restart();
    }
    if (!(bits & WIFI_CONNECTED)) {
        write_wifi_log("Timeout 20s", wifi_retry);
        printf("[WiFi] Timeout 20s. Kiem tra router/tin hieu. Restart sau 10s...\n");
        vTaskDelay(pdMS_TO_TICKS(10000));  /* Tranh restart loop lien tuc */
        esp_restart();
    }
}

static void weather_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(2000));  // Cho WiFi on dinh

    while (1) {
        float t = fetch_hcm_temperature();
        if (t > -900.0f) {
            printf("\n=== HCM City ===\nNhiet do: %.1f C\n", t);
        } else {
            printf("\n[Weather] Loi lay du lieu\n");
        }
        vTaskDelay(pdMS_TO_TICKS(WEATHER_MS));
    }
}

static void blink_task(void *arg)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Tim thanh pho theo ten (khong phan biet hoa thuong). Ho tro alias: HCM, NY, SG
static const city_t* find_city(const char *name)
{
    if (!name || !*name) return NULL;
    for (int i = 0; i < (int)NUM_CITIES; i++) {
        if (strcasecmp(name, CITIES[i].name) == 0) return &CITIES[i];
    }
    if (strcasecmp(name, "HCM") == 0) return &CITIES[0];
    if (strcasecmp(name, "NY") == 0) return &CITIES[3];
    if (strcasecmp(name, "SG") == 0) return &CITIES[7];  // Singapore
    return NULL;
}

// Trim khoang trang dau/cuoi
static void trim(char *s)
{
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    char *end = p + strlen(p);
    while (end > p && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static void serial_task(void *arg)
{
    /* Tat buffer de text nhap hien thi ngay (linenoise cung echo) */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    linenoiseSetDumbMode(1);  /* Dumb mode = echo tung ky tu */

    printf("\nDanh sach: HCM City, Tokyo, London, New York, Paris, Sydney, Dubai, Singapore\n\n");

    while (1) {
        char *line = linenoise("Nhap ten thanh pho: ");
        if (!line) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        trim(line);
        if (strlen(line) > 0) {
            const city_t *city = find_city(line);
            if (city) {
                printf("  %s: ", city->name);
                fflush(stdout);
                float temp = fetch_temp(city->lat, city->lon);
                if (temp > -900.0f) {
                    printf("%.1f C\n", temp);
                } else {
                    printf("Loi lay du lieu\n");
                }
            } else {
                printf("  Khong tim thay '%s'.\n", line);
            }
        }
        linenoiseFree(line);
    }
}

static void print_wifi_log_if_exists(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = false
    };
    if (esp_vfs_spiffs_register(&conf) != ESP_OK) return;

    FILE *f = fopen(LOG_PATH, "r");
    if (f) {
        printf("\n--- WiFi log (lan truoc) ---\n");
        char buf[128];
        while (fgets(buf, sizeof(buf), f)) printf("%s", buf);
        printf("---\n");
        fclose(f);
    }
    esp_vfs_spiffs_unregister("storage");
}

extern "C" void app_main(void)
{
    printf("\n===== ESP32-C3 - Nhiet do cac thanh pho =====\n");

    ESP_ERROR_CHECK(nvs_flash_init());
    print_wifi_log_if_exists();
    wifi_init();

    xTaskCreate(blink_task, "blink", 2048, NULL, 1, NULL);
    xTaskCreate(weather_task, "weather", 4096, NULL, 2, NULL);
    xTaskCreate(serial_task, "serial", 8192, NULL, 1, NULL);
    start_web_server();
}
