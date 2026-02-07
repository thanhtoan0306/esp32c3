/**
 * ESP32-C3 Supermini - Crypto Price Display
 * Hiển thị giá crypto qua serial, mỗi 2s một coin mới
 */

#include <stdio.h>
#include <string.h>
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

#define WIFI_CONNECTED  BIT0
#define WIFI_FAILED     BIT1
#define WIFI_TIMEOUT_MS 30000  // 30 giây timeout
#define HTTP_BUFFER     1024
#define CRYPTO_INTERVAL 2000   // 2 giây

static const char *TAG = "crypto";
static EventGroupHandle_t wifi_event_group;
static int wifi_retry_count = 0;

// Danh sách crypto - mỗi 2s hiển thị 1 coin
struct CryptoInfo {
    const char *id;   // CoinGecko API id
    const char *name; // Tên hiển thị
};
static const CryptoInfo cryptos[] = {
    {"bitcoin", "BTC"},
    {"ethereum", "ETH"},
    {"solana", "SOL"},
    {"cardano", "ADA"},
    {"ripple", "XRP"},
    {"dogecoin", "DOGE"},
    {"avalanche-2", "AVAX"},
    {"polkadot", "DOT"},
};
#define NUM_CRYPTOS (sizeof(cryptos) / sizeof(cryptos[0]))

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
            if (offset + copy_len >= HTTP_BUFFER) copy_len = HTTP_BUFFER - offset - 1;
            if (copy_len > 0) {
                memcpy((char *)evt->user_data + offset, evt->data, copy_len);
                offset += copy_len;
                ((char *)evt->user_data)[offset] = '\0';
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        offset = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        offset = 0;
        break;
    default:
        break;
    }
    return ESP_OK;
}

static float fetch_crypto_price(const char *crypto_id)
{
    char url[128];
    snprintf(url, sizeof(url),
        "https://api.coingecko.com/api/v3/simple/price?ids=%s&vs_currencies=usd",
        crypto_id);

    char response[HTTP_BUFFER] = {0};

    esp_http_client_config_t config = {0};
    config.url = url;
    config.event_handler = http_event_handler;
    config.user_data = response;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 10000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return -1.0f;

    esp_err_t err = esp_http_client_perform(client);
    float price = -1.0f;

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            cJSON *root = cJSON_Parse(response);
            if (root) {
                cJSON *coin = cJSON_GetObjectItem(root, crypto_id);
                if (coin) {
                    cJSON *usd = cJSON_GetObjectItem(coin, "usd");
                    if (cJSON_IsNumber(usd)) {
                        price = (float)usd->valuedouble;
                    }
                }
                cJSON_Delete(root);
            }
        }
    }

    esp_http_client_cleanup(client);
    return price;
}

static const char* wifi_reason_str(uint8_t reason)
{
    switch (reason) {
        case 1: return "Unspecified";
        case 2: return "Auth expired (sai mat khau?)";
        case 3: return "Auth leave";
        case 4: return "Assoc expire";
        case 5: return "Too many stations";
        case 6: return "Not authenticated";
        case 7: return "Not associated";
        case 8: return "Assoc leave";
        case 9: return "Assoc not authed";
        case 15: return "4-way handshake timeout (sai mat khau?)";
        case 201: return "No AP found (khong tim thay WiFi)";
        case 202: return "Auth fail (sai mat khau)";
        case 204: return "Handshake timeout";
        default: return "Unknown";
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *evt = (wifi_event_sta_disconnected_t *)event_data;
        wifi_retry_count++;
        printf("[WiFi] Disconnect #%d - reason %d (%s)\n", wifi_retry_count, evt->reason, wifi_reason_str(evt->reason));

        if (wifi_retry_count > 10) {
            ESP_LOGE(TAG, "Qua nhieu lan that bai! Kiem tra SSID/mat khau trong .env");
            xEventGroupSetBits(wifi_event_group, WIFI_FAILED);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));  // Nghi 1s truoc khi retry
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi OK! IP: " IPSTR, IP2STR(&event->ip_info.ip));
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

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi %s...", WIFI_SSID);
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
        WIFI_CONNECTED | WIFI_FAILED, pdFALSE, pdFALSE, pdMS_TO_TICKS(WIFI_TIMEOUT_MS));

    if (bits & WIFI_FAILED) {
        printf("\n[WiFi] FAILED - Sai mat khau? Kiem tra .env\n");
        esp_restart();
    }
    if (!(bits & WIFI_CONNECTED)) {
        printf("\n[WiFi] Timeout %ds - Kiem tra router, tin hieu\n", WIFI_TIMEOUT_MS/1000);
        esp_restart();
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Crypto Price Display - Moi 2s 1 coin");
    printf("\n========== CRYPTO PRICE (USD) ==========\n");

    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();

    uint32_t idx = 0;
    while (1) {
        const CryptoInfo *c = &cryptos[idx % NUM_CRYPTOS];
        float price = fetch_crypto_price(c->id);

        if (price >= 0) {
            printf("[%s] $%.2f\n", c->name, price);
        } else {
            printf("[%s] Error\n", c->name);
        }

        idx++;
        vTaskDelay(pdMS_TO_TICKS(CRYPTO_INTERVAL));
    }
}
