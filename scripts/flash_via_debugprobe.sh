#!/bin/bash
# flash_via_debugprobe.sh - Build + program. Si `expect` está instalado usa la
# contraseña de SUDO_PASSWORD para automatizar sudo; si no, la pide interactiva.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

echo "=== ${PROJECT} Firmware - Build & Program via Debug Probe ==="
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

if command -v expect >/dev/null 2>&1; then
    echo "Using expect for automated sudo..."
    expect <<EOF
spawn sudo -E LD_LIBRARY_PATH="${HIDAPI_LIB}" "${OPENOCD_BIN}" -s "${OPENOCD_SCRIPTS}" -f "${CONFIG_FILE}" -c "program ${ELF_FILE} verify reset exit"
expect "password for"
send "$env(SUDO_PASSWORD)\r"
expect eof
EOF
else
    echo "expect no instalado. Se usará sudo interactivo."
    echo "Para automatizar: sudo apt install expect"
    echo ""
    sudo LD_LIBRARY_PATH="${HIDAPI_LIB}" "${OPENOCD_BIN}" \
        -s "${OPENOCD_SCRIPTS}" \
        -f "${CONFIG_FILE}" \
        -c "program ${ELF_FILE} verify reset exit"
fi

echo ""
echo "=== Programming complete! ==="