# ESP32-C3 Supermini - Blink Built-in LED

Dự án C++ blink LED tích hợp trên bo ESP32-C3 Supermini (OLED board).

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

---

## Cách build và flash (từng bước)

```bash
./build_flash.sh          # Build + Flash
./run_monitor.sh          # Mở Serial Monitor
```

## Cấu trúc dự án

```
esp32c3/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   └── blink_main.cpp
└── README.md
```
