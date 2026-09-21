#!/bin/bash
# scripts/diag_flash.sh — Diagnóstico + programación SWD con sudo.
#
# Hace las 3 verificaciones REALES (no inventadas) y SOLO entonces programa:
#   1) probe SWD            lsusb 2e8a:000c (Raspberry Pi Debugprobe)
#   2) build [100%] + ELF   ninja REAL de $(PROJECT) (heredado de config.sh)
#   3) ELF > 150 KB         flasheable por SWD; si VACÍO → lo dice, NO flashea
#
# PROGRAMACIÓN: openocd + debugprobe.cfg (CMSIS-DAP 2e8a:000c) con sudo
# (permisos de dispositivo), verify + reset + exit. La ÚNICA línea que manda:
#   [100%] Built target + ELF>150KB → "program verify reset exit" ≈ OK.
#   SIN ELF → imprime la ÚNICA línea honesta y EXIT 1 (no inventa un [100%]).

set -eo pipefail

cd "$(dirname "$0")/.."
source scripts/config.sh

echo "=== [diag_flash] diagnóstico REAL ==="
echo "proyecto : ${PROJECT:-keyboard_oled}"
echo "builddir : ${BUILD_DIR:-${PROJECT_DIR}/build}"

# 1 — probe
if lsusb | grep -qE "2e8a:000c"; then
    echo "probe    : OK (2e8a:000c CMSIS-DAP)"
else
    echo "probe    : NO CONECTADO (necesitas la Debugprobe 2e8a:000c por USB)"
    exit 1
fi

# 2 — build REAL
scripts/build.sh 2>&1 | tail -3
ELF=$(find "${BUILD_DIR:-${PROJECT_DIR}/build}/src" -maxdepth 1 \
      -name "pico_keyboard_bridge.elf" -exec ls -l {} \; 2>/dev/null \
      | awk '$5>150000{print $5}')

# 3 — veredicto ÚNICO honesto
if [ -n "$ELF" ]; then
    KB=$((ELF / 1024))
    echo "ELF      : ${KB} KB → PROGRAMO SWD AHORA (sudo openocd verify reset)"
    sudo openocd -f debugprobe.cfg -c "program ${ELF} verify reset exit"
else
    echo "ELF      : SIN ELF>150KB → NO programo (la regla: si no hay [100%] lo digo)"
    exit 1
fi
