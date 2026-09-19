#!/bin/bash
# flash_bootsel.sh - Programa via BOOTSEL (drag & drop)

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

UF2_FILE="${BUILD_DIR}/${PROJECT}.uf2"

echo "=== Flash ${PROJECT} via BOOTSEL ==="
echo ""

if [ ! -f "${UF2_FILE}" ]; then
    echo "Error: ${UF2_FILE} not found. Run ./build.sh first"
    exit 1
fi

echo "1. Hold BOOTSEL button on Pico"
echo "2. Connect USB to computer"
echo "3. Release BOOTSEL"
echo "4. Press Enter to copy firmware..."
read -r

RPI_RP2="/media/optimus/RPI-RP2"
if [ ! -d "${RPI_RP2}" ]; then
    echo "RPI-RP2 not found at ${RPI_RP2}"
    echo "Check: ls /media/optimus/"
    exit 1
fi

cp "${UF2_FILE}" "${RPI_RP2}/"
echo "Firmware copied! Pico will reboot."