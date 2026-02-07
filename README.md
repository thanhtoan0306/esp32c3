# ESP32-C3 Supermini - WiFi + Nhiet do cac thanh pho

- Ket noi WiFi
- **Web UI:** Mo `http://<IP>` tren trinh duyet (cung WiFi) - dropdown chon thanh pho, nut tra cuu
- Serial: nhap ten thanh pho + Enter

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

Khi ESP32 ket noi WiFi, xem IP tren Serial. Mo `http://<IP>` tren trinh duyet de dung giao dien web.

---

## Cách build và flash (từng bước)

```bash
./build_flash.sh          # Build + Flash
./run_monitor.sh          # Mở Serial Monitor
```

## Giao dien Web

Sau khi ESP32 ket noi WiFi, Serial se hien IP (vd: `192.168.1.10`). Mo trinh duyet:

```
http://192.168.1.10/
```

Giao dien HTML: chon thanh pho tu dropdown, nhan "Tra cuu" de xem nhiet do.

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
