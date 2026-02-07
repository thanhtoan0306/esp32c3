idf_component_register(
    SRCS "blink_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver json esp_wifi esp_http_client esp_http_server esp_netif esp_event nvs_flash mbedtls spiffs console
)
