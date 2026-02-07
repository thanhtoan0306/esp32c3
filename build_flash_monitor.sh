#!/bin/bash
# Build + Nạp + Mở Serial Monitor - 1 lệnh duy nhất
# Chạy: ./build_flash_monitor.sh

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

echo "🔧 Build → Flash → Monitor"
idf.py set-target esp32c3
idf.py -p "$PORT" flash monitor
