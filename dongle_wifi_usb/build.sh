#!/bin/bash

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"

echo "=== Building dongle_wifi_usb ==="
echo "Project: ${PROJECT_DIR}"
echo "Build:   ${BUILD_DIR}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

export PICO_SDK_PATH="${PICO_SDK_PATH:-${PROJECT_DIR}/../../pico-sdk}"

cmake -DPICO_BOARD=pico_w \
      -DCMAKE_BUILD_TYPE=Release \
      "${PROJECT_DIR}"

make -j$(nproc)

echo ""
echo "=== Build complete ==="
echo "UF2: ${BUILD_DIR}/dongle_wifi_usb.uf2"
echo ""
echo "To flash:"
echo "  1. Hold BOOTSEL on Pico, connect USB"
echo "  2. cp ${BUILD_DIR}/dongle_wifi_usb.uf2 /media/$(whoami)/RPI-RP2/"
echo ""
echo "Or via SSH to Raspberry Pi:"
echo "  ssh joy@raspberry.local \"cd /home/joy/src/pico/pico_debugger_flash && git pull\""
echo "  Then copy UF2 to the Pi and flash from there"