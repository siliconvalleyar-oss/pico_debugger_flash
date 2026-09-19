#!/bin/bash
# flash_nosudo.sh - Compila y graba el firmware de pico_usb_drive_configurable SIN sudo.
# Prerrequisito: regla udev instalada (scripts/install_udev.sh del repo raíz) y sonda reconectada.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Fijamos el proyecto y su layout de build (los binarios quedan en build/src/
# porque el CMake raíz hace add_subdirectory(src)).
export PROJECT="pico_usb_drive_configurable"
source "${REPO_ROOT}/scripts/config.sh"

BUILD_DIR="${PROJECT_DIR}/build"
ELF_FILE="${BUILD_DIR}/src/${PROJECT}.elf"

echo "=== ${PROJECT} Firmware - Build & Program (no sudo) ==="
echo "Board: ${BOARD:-pico}   (use BOARD=pico_w para Pico W)"
echo ""
print_toolchain
echo ""

if [ ! -r "${CONFIG_FILE}" ]; then
    echo "Error: ${CONFIG_FILE} no existe"
    exit 1
fi

"${REPO_ROOT}/scripts/build.sh"

if [ ! -f "${ELF_FILE}" ]; then
    echo "Error: Build failed - ${ELF_FILE} not found"
    exit 1
fi

echo ""
echo "Step 2: Programming via Debug Probe (SWD)..."
echo "ELF: ${ELF_FILE}"
echo ""

# reset halt antes de programar: si el target estuviera ejecutando el firmware,
# su uso de RAM choca con el buffer de programacion de OpenOCD y la grabacion
# falla a mitad ("Failed to write memory at 0x2001....").
run_openocd "${CONFIG_FILE}" -c "reset halt; program ${ELF_FILE} verify reset exit"

echo ""
echo "=== Programming complete! ==="