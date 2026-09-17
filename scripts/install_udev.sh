#!/bin/bash
# install_udev.sh - Instala la regla udev que permite usar el Debug Probe (y
# cualquier RP2040 2e8a) SIN sudo. Debe ejecutarse con sudo (una sola vez).
#
#   sudo ./scripts/install_udev.sh
#   # luego DESCONECTAR y RECONECTAR la sonda para que la regla se aplique.

set -euo pipefail

RULE='SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", MODE="0666", GROUP="plugdev"'
RULE_FILE="/etc/udev/rules.d/99-pico-debugprobe.rules"

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: ejecútelo con sudo:  sudo $0"
    exit 1
fi

echo "$RULE" > "${RULE_FILE}"
udevadm control --reload-rules
udevadm trigger

echo "Regla udev instalada en ${RULE_FILE}"
echo ""
echo "IMPORTANTE: desconecte y reconecte el Pico sonda para que el nodo USB"
echo "tome los nuevos permisos (el usuario debe pertenecer al grupo plugdev o"
echo "confiar en MODE=0666)."
echo ""
echo "Verificación:"
echo "  lsusb | grep 2e8a"
echo "  ls -la /dev/bus/usb/$(lsusb | grep 2e8a | awk '{print $2}')/$(lsusb | grep 2e8a | awk '{print $4}' | tr -d ':')"