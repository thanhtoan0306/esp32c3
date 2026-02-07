idf_component_register(
    SRCS "blink_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES json esp_wifi esp_http_client esp_netif esp_event nvs_flash mbedtls
)
