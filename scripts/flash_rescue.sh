#!/bin/bash
# scripts/flash_rescue.sh — Modo RESCUE (target "colgado", lección 8.6).
#
# Cuando un firmware vivo rompe el SWD (o el target no conecta), el modo
# rescue nativo de target/rp2040.cfg resetea el PSM del RP2040 (clear
# DBGPWRUPREQ) dejando el core halted en el bootrom, listo para reprogramar.
#
# FASE 1: OpenOCD con CONFIG_RESCUE_FILE (= set RESCUE 1 + target/rp2040.cfg).
#         Éxito: "SWD DPIDR 0x10212927, DLPIDR 0xf0000001" +
#                "Now restart OpenOCD without RESCUE flag" (+ shutdown propio).
# FASE 2: reprograma el firmware normal delegando en flash_nosudo_multi.sh
#         (mismo BOARD/PROJECT del entorno; argumentos pasan tal cual).
#
# Uso:
#   ./scripts/flash_rescue.sh                              # rescue + menú de proyectos
#   ./scripts/flash_rescue.sh pico-ble-keyboard-bridge     # rescue + reprogramar ese proyecto
#   BOARD=pico_w ./scripts/flash_rescue.sh <proyecto>      # Pico W
#   ./scripts/flash_rescue.sh --rescue-only [proyecto]     # solo rescue, sin fase 2
#   RESCUE_ONLY=1 ./scripts/flash_rescue.sh                # equivalente al flag

set -euo pipefail

cd "$(dirname "$0")/.."
source scripts/config.sh

RESCUE_ONLY=0
ARGS=()
for a in "$@"; do
    if [ "$a" = "--rescue-only" ]; then
        RESCUE_ONLY=1
    else
        ARGS+=("$a")
    fi
done

echo "=== [flash_rescue] modo RESCUE (target colgado) ==="
print_toolchain
echo ""

# --- 0 — sonda presente (como diag_flash.sh) -------------------------------
if ! lsusb | grep -qE "2e8a:000c"; then
    echo "probe    : NO CONECTADO (necesitas la Debugprobe 2e8a:000c por USB)"
    exit 1
fi
echo "probe    : OK (2e8a:000c CMSIS-DAP)"

[ -f "${CONFIG_RESCUE_FILE}" ] || { echo "config rescue no encontrada: ${CONFIG_RESCUE_FILE}"; exit 1; }

# --- FASE 1 — rescue nativo -------------------------------------------------
LOG="$(mktemp /tmp/flash_rescue.XXXXXX.log)"
echo "rescue   : ${CONFIG_RESCUE_FILE}"
echo "           (log: ${LOG})"

set +e
run_openocd "${CONFIG_RESCUE_FILE}" 2>&1 | tee "${LOG}"
rc=${PIPESTATUS[0]}
set -e

if grep -q "Now restart OpenOCD without RESCUE flag" "${LOG}" \
   && grep -q "SWD DPIDR" "${LOG}"; then
    echo "rescue   : OK — PSM reseteado, core halted en bootrom"
else
    echo "rescue   : FALLÓ (rc=${rc}) — últimas líneas del log:"
    tail -8 "${LOG}" || true
    echo "           (síntoma clásico: '-tap is invalid' = cfg sin rescue nativo;"
    echo "            'Rescue failed, DP CTRL/STAT' = sonda/target sin alimentar)"
    exit 1
fi

# --- FASE 2 — reprogramar firmware normal -----------------------------------
if [ "${RESCUE_ONLY}" = "1" ]; then
    echo "fase 2   : omitida (--rescue-only). Reprogramá con flash_nosudo_multi.sh."
    exit 0
fi

echo ""
echo "fase 2   : reprogramando firmware normal (flash_nosudo_multi.sh)..."
exec scripts/flash_nosudo_multi.sh "${ARGS[@]+"${ARGS[@]}"}"
