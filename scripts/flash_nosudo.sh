#!/bin/bash
# flash_nosudo.sh - Compila y programa el target SIN sudo.
# Prerrequisito: regla udev instalada (scripts/install_udev.sh) y sonda reconectada.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

echo "=== Blink Firmware - Build & Program (no sudo) ==="
echo ""
print_toolchain
echo ""

if [ ! -r "${CONFIG_FILE}" ]; then
    echo "Error: ${CONFIG_FILE} no existe"
    exit 1
fi

"${SCRIPT_DIR}/build.sh"

if [ ! -f "${ELF_FILE}" ]; then
    echo "Error: Build failed - ${ELF_FILE} not found"
    exit 1
fi

echo ""
echo "Step 2: Programming via Debug Probe (SWD)..."
echo "ELF: ${ELF_FILE}"
echo ""

run_openocd "${CONFIG_FILE}" -c "program ${ELF_FILE} verify reset exit"

echo ""
echo "=== Programming complete! ==="