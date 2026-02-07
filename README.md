# ESP32-C3 Supermini - WiFi + Nhiet do HCM City

- Ket noi WiFi
- Lay nhiet do HCM qua Open-Meteo API
- Hien thi qua Serial (cap nhat moi 30s)

## Phần cứng

- **Bo mạch:** ESP32-C3 Supermini với OLED 0.42 inch
- **LED tích hợp:** GPIO8 (LED đỏ giữa nút RST và BOOT)

## Yêu cầu

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/) đã cài đặt

## Chạy nhanh (1 lệnh duy nhất)

```bash
./build_flash_monitor.sh
```

Build → nạp code → mở Serial Monitor. Thoát monitor: **Ctrl+]**

Output: Nhiet do HCM City hien thi moi 30 giay.

---

## Cách build và flash (từng bước)

```bash
./build_flash.sh          # Build + Flash
./run_monitor.sh          # Mở Serial Monitor
```

## WiFi (.env)

Sua `main/wifi_config.h` hoac tao tu `.env`:

```env
WIFI_SSID=your_wifi
WIFI_PASSWORD=your_password
```

## Cấu trúc dự án

```
esp32c3/
├── .env                 # WiFi credentials
├── main/
│   ├── blink_main.cpp
│   └── wifi_config.h
└── README.md
```
