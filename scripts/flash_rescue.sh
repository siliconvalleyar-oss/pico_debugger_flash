#!/bin/bash
# flash_rescue.sh - Intenta conectar en modo RESCUE cuando el target no responde
# por SWD normal (código corrupto / en conflicto). Usa debugprobe-openocd-rescue.cfg.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

if [ ! -f "${ELF_FILE}" ]; then
    echo "Error: ${ELF_FILE} no existe. Ejecute ./build.sh primero"
    exit 1
fi

echo "=== Programming Pico via Debug Probe (Rescue Mode) ==="
echo "ELF: ${ELF_FILE}"
echo ""

echo "Aviso: este modo requiere que el RP2040 del target tenga su PSM reseteado"
echo "vía el bit DBGPWRUPREQ. Asegúrese del cableado SWD correcto."
echo ""

echo "Esta operación usa sudo (pide contraseña)..."
sudo -v

echo ""
echo "Conectando al target Pico via Debug Probe..."
echo "Cableado: GP2->GP2 (SWDIO), GP3->GP3 (SWCLK), GND->GND"
echo ""

sudo LD_LIBRARY_PATH="${HIDAPI_LIB}" "${OPENOCD_BIN}" \
    -s "${OPENOCD_SCRIPTS}" \
    -f "${CONFIG_RESCUE_FILE}" \
    -c "program ${ELF_FILE} verify reset exit"

echo ""
echo "=== Done! ==="