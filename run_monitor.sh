#!/bin/bash
# Mở serial monitor xem trạng thái LED ON/OFF
# Chạy trong terminal: ./run_monitor.sh

export PATH="/opt/homebrew/opt/python@3.12/libexec/bin:$PATH"
source ~/.espressif/v5.5.2/esp-idf/export.sh

cd "$(dirname "$0")"
PORT="${1:-/dev/cu.usbmodem101}"

echo "📺 Serial Monitor - LED ON/OFF"
echo "   Thoát: Ctrl+]"
echo ""

idf.py -p "$PORT" monitor
