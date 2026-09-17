#!/bin/bash
# flash_simple.sh - Build + program. Usa sudo solo si la udev no está instalada
# (sudo -v refresca el ticket de contraseña una sola vez).

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

echo "=== Blink Firmware - Build & Program via Debug Probe ==="
echo "Board: ${BOARD:-pico}   (use BOARD=pico_w para Pico W)"
echo ""

if [ ! -x "${OPENOCD_BIN}" ]; then
    echo "Error: openocd no encontrado (${OPENOCD_BIN})"
    exit 1
fi

if [ ! -r "${CONFIG_FILE}" ]; then
    echo "Error: config no encontrada (${CONFIG_FILE})"
    exit 1
fi

echo "Step 1: Building firmware..."
"${SCRIPT_DIR}/build.sh"

if [ ! -f "${ELF_FILE}" ]; then
    echo "Error: Build failed - ${ELF_FILE} not found"
    exit 1
fi

echo ""
echo "Step 2: Programming via Debug Probe (SWD)..."
echo "ELF: ${ELF_FILE}"
echo ""

sudo -v
sudo LD_LIBRARY_PATH="${HIDAPI_LIB}" "${OPENOCD_BIN}" \
    -s "${OPENOCD_SCRIPTS}" \
    -f "${CONFIG_FILE}" \
    -c "program ${ELF_FILE} verify reset exit"

echo ""
echo "=== Programming complete! ==="