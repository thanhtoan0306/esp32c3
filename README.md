# ESP32-C3 Supermini - Blink Built-in LED

Dự án C++ blink LED tích hợp trên bo ESP32-C3 Supermini (OLED board).

## Phần cứng

- **Bo mạch:** ESP32-C3 Supermini với OLED 0.42 inch
- **LED tích hợp:** GPIO8 (LED đỏ giữa nút RST và BOOT)

## Yêu cầu

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/) đã cài đặt

## Cách build và flash

```bash
# Kích hoạt môi trường ESP-IDF
source $IDF_PATH/export.sh

# Vào thư mục dự án
cd /Users/duongthanhtoan/Desktop/codespace/esp32c3

# Chọn target ESP32-C3 (chỉ cần lần đầu)
idf.py set-target esp32c3

# Build
idf.py build

# Flash (thay /dev/cu.usbmodem* bằng port COM của bạn)
idf.py -p /dev/cu.usbmodem* flash

# Xem log serial
idf.py -p /dev/cu.usbmodem* monitor
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
