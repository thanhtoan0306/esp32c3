# ESP32-C3 Supermini - Crypto Price Display

Dự án C++ hiển thị giá crypto qua serial (mỗi 2 giây 1 coin mới).

## Phần cứng

- **Bo mạch:** ESP32-C3 Supermini với OLED 0.42 inch
- **Chức năng:** Hiển thị giá crypto (BTC, ETH, SOL, ADA, XRP, DOGE, AVAX, DOT) qua serial mỗi 2 giây

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

# Xem serial monitor (giá crypto)
./run_monitor.sh
```

## WiFi credentials (.env)

Thông tin WiFi lưu trong `.env` (không commit lên git):

```env
WIFI_SSID=your_wifi_name
WIFI_PASSWORD=your_wifi_password
```

- Copy `.env.example` thành `.env` rồi điền SSID và mật khẩu
- Mỗi lần build, `main/wifi_config.h` được tạo từ `.env`
- Trong code: `#include "wifi_config.h"` rồi dùng `WIFI_SSID` và `WIFI_PASSWORD`

## Cấu trúc dự án

```
esp32c3/
├── .env                 # WiFi credentials (gitignore)
├── .env.example         # Mẫu
├── CMakeLists.txt
├── scripts/
│   └── gen_wifi_config.py
├── main/
│   ├── blink_main.cpp
│   └── wifi_config.h   # Auto-generated từ .env
└── README.md
```
