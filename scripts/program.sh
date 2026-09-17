#!/bin/bash
# program.sh - Programa el Pico target vía Debug Probe usando sudo
# (requiere contraseña si la regla udev no está instalada -> ver install_udev.sh).

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

if [ ! -f "${ELF_FILE}" ]; then
    echo "Error: ${ELF_FILE} no existe. Ejecute ./build.sh primero"
    exit 1
fi

echo "Programming Pico via Debug Probe (CMSIS-DAP)..."
echo "ELF: ${ELF_FILE}"
echo "OpenOCD: ${OPENOCD_BIN}"
echo ""

sudo LD_LIBRARY_PATH="${HIDAPI_LIB}" "${OPENOCD_BIN}" \
    -s "${OPENOCD_SCRIPTS}" \
    -f "${CONFIG_FILE}" \
    -c "program ${ELF_FILE} verify reset exit"

echo ""
echo "Programming complete!"

echo ""
echo "Tip: instale la regla udev para no usar sudo:"
echo "  sudo ./scripts/install_udev.sh  (y reconecte la sonda)"
echo "  luego use ./scripts/flash_nosudo.sh"