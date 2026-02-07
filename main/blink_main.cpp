/**
 * ESP32-C3 Supermini - WiFi + Nhiet do cac thanh pho
 * - Ket noi WiFi
 * - Lay nhiet do HCM qua Open-Meteo API (tu dong 30s)
 * - Nhan Enter: lay nhiet do 8 thanh pho lon, ghi log vao /spiffs/temp_log.txt
 */

#include <stdio.h>
#include <string.h>
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
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "wifi_config.h"
#include "esp_spiffs.h"
#include <sys/time.h>

#define LED_GPIO       GPIO_NUM_8
#define LOG_PATH       "/spiffs/wifi_log.txt"
#define TEMP_LOG_PATH  "/spiffs/temp_log.txt"
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

static void fetch_and_log_all_cities(void)
{
    printf("\n=== Nhiet do cac thanh pho lon ===\n");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = true
    };
    if (esp_vfs_spiffs_register(&conf) != ESP_OK) {
        printf("[Log] Loi mount SPIFFS\n");
        return;
    }

    FILE *flog = fopen(TEMP_LOG_PATH, "a");
    if (!flog) {
        esp_vfs_spiffs_unregister("storage");
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm t;
    time_t now = tv.tv_sec;
    localtime_r(&now, &t);
    fprintf(flog, "\n--- [%04d-%02d-%02d %02d:%02d:%02d] ---\n",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);

    for (int i = 0; i < (int)NUM_CITIES; i++) {
        float temp = fetch_temp(CITIES[i].lat, CITIES[i].lon);
        if (temp > -900.0f) {
            printf("  %-12s: %6.1f C\n", CITIES[i].name, temp);
            fprintf(flog, "%s: %.1f C\n", CITIES[i].name, temp);
        } else {
            printf("  %-12s: ---\n", CITIES[i].name);
            fprintf(flog, "%s: (loi)\n", CITIES[i].name);
        }
        vTaskDelay(pdMS_TO_TICKS(300));  // Tranh flood API
    }

    fclose(flog);
    esp_vfs_spiffs_unregister("storage");
    printf("[Log] Da ghi vao %s\n", TEMP_LOG_PATH);
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
            printf("[WiFi] FAILED - Kiem tra SSID/mat khau trong wifi_config.h\n");
            xEventGroupSetBits(wifi_event_group, WIFI_FAILED);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
        printf("[WiFi] OK! IP: " IPSTR "\n", IP2STR(&evt->ip_info.ip));
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
        printf("[WiFi] Qua nhieu lan that bai. Restart...\n");
        esp_restart();
    }
    if (!(bits & WIFI_CONNECTED)) {
        write_wifi_log("Timeout 20s", wifi_retry);
        printf("[WiFi] Timeout 20s. Kiem tra router/tin hieu. Restart...\n");
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

static void serial_task(void *arg)
{
    char buf[64];
    printf("\nNhan Enter de lay nhiet do cac thanh pho lon...\n");
    while (1) {
        if (fgets(buf, sizeof(buf), stdin) != NULL) {
            fetch_and_log_all_cities();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
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
}
