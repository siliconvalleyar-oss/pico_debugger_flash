#!/bin/bash
# flash_nosudo.sh - Compila y programa el target SIN sudo.
# Prerrequisito: regla udev instalada (scripts/install_udev.sh) y sonda reconectada.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

echo "=== ${PROJECT} Firmware - Build & Program (no sudo) ==="
echo "Board: ${BOARD:-pico}   (use BOARD=pico_w para Pico W)"
echo ""
print_toolchain
echo ""

if [ ! -r "${CONFIG_FILE}" ]; then
    echo "Error: ${CONFIG_FILE} no existe"
    exit 1
fi

"${SCRIPT_DIR}/build.sh"

# Buscar ELF si no está en la ruta esperada (puede estar en build/ o build/src/)
if [ ! -f "${ELF_FILE}" ]; then
    FOUND_ELF=$(find "${BUILD_DIR}" -name "${PROJECT_CMAKE_TARGET}.elf" -print -quit 2>/dev/null || true)
    if [ -n "${FOUND_ELF}" ]; then
        ELF_FILE="${FOUND_ELF}"
        UF2_FILE="${ELF_FILE%.elf}.uf2"
    else
        echo "Error: Build failed - ${PROJECT_CMAKE_TARGET}.elf not found in ${BUILD_DIR}"
        exit 1
    fi
fi

echo ""
echo "Step 2: Programming via Debug Probe (SWD)..."
echo "ELF: ${ELF_FILE}"
echo ""

run_openocd "${CONFIG_FILE}" -c "program ${ELF_FILE} verify reset exit"

echo ""
echo "=== Programming complete! ==="