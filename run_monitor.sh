#!/bin/bash
# Mở serial monitor - nhập tên thành phố tra nhiệt độ
# Chạy trong terminal: ./run_monitor.sh
#
# Nếu không thấy text khi gõ (idf_monitor lỗi echo), dùng minicom:
#   ./run_monitor.sh minicom
#   (cài: brew install minicom)

export PATH="/opt/homebrew/opt/python@3.12/libexec/bin:$PATH"
source ~/.espressif/v5.5.2/esp-idf/export.sh

cd "$(dirname "$0")"
PORT="${1:-/dev/cu.usbmodem101}"

if [ "$1" = "minicom" ]; then
  echo "📺 Minicom - text nhap hien thi day du"
  echo "   Thoát: Ctrl+A, X"
  echo ""
  minicom -D /dev/cu.usbmodem101 -b 115200
else
  echo "📺 IDF Monitor"
  echo "   Thoát: Ctrl+]"
  echo "   Neu ko thay text khi go: ./run_monitor.sh minicom"
  echo ""
  idf.py -p "$PORT" monitor
fi
