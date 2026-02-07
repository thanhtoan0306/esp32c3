#!/bin/bash
# Build và nạp code lên ESP32-C3
# Chạy: ./build_flash.sh

set -e

# Kiểm tra ESP-IDF
if [ -z "$IDF_PATH" ]; then
    echo "❌ ESP-IDF chưa được kích hoạt."
    echo ""
    echo "Chạy lệnh sau trước (chọn 1 trong các đường dẫn phù hợp):"
    echo "  source \$HOME/esp/esp-idf/export.sh"
    echo "  # hoặc nếu cài qua VSCode extension:"
    echo "  source \$HOME/.espressif/esp-idf/export.sh"
    echo ""
    exit 1
fi

cd "$(dirname "$0")"
PORT="${1:-/dev/cu.usbmodem101}"

echo "🔧 Target: esp32c3"
idf.py set-target esp32c3

echo "📦 Building..."
idf.py build

echo "📤 Flashing to $PORT..."
idf.py -p "$PORT" flash

echo "✅ Hoàn tất! LED sẽ nhấp nháy."
echo "   Chạy 'idf.py -p $PORT monitor' để xem log serial."
