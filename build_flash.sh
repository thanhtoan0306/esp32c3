#!/bin/bash
# Build và nạp code lên ESP32-C3
# Chạy: ./build_flash.sh

set -e

# Tự kích hoạt ESP-IDF nếu chưa có
if [ -z "$IDF_PATH" ]; then
    if [ -f "$HOME/.espressif/v5.5.2/esp-idf/export.sh" ]; then
        export PATH="/opt/homebrew/opt/python@3.12/libexec/bin:$PATH"
        source "$HOME/.espressif/v5.5.2/esp-idf/export.sh"
    else
        echo "❌ ESP-IDF chưa được cài đặt."
        echo "   Chạy: brew install eim && eim install"
        exit 1
    fi
fi

cd "$(dirname "$0")"
PORT="${1:-/dev/cu.usbmodem101}"

echo "🔧 Target: esp32c3"
idf.py set-target esp32c3

echo "📦 Building..."
idf.py build

echo "📤 Flashing to $PORT..."
idf.py -p "$PORT" flash

echo "✅ Hoàn tất!"
echo "   Chạy 'idf.py -p $PORT monitor' để xem log serial."
