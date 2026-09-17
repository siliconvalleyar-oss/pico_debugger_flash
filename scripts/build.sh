#!/bin/bash
# build.sh - Compila el firmware blink para Pico 1 (RP2040).
# Requiere: cmake, arm-none-eabi-gcc y PICO_SDK_PATH (auto o variable de entorno).

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

echo "Building blink firmware for Pico 1 (RP2040)..."
echo "Project: ${BLINK_DIR}"
echo "Build:   ${BUILD_DIR}"
echo "SDK:     ${PICO_SDK_PATH:-<no definido>}"

if [ -z "${PICO_SDK_PATH:-}" ]; then
    echo "Error: PICO_SDK_PATH no está definido y no se encontró ../pico-sdk"
    echo "Exporte la variable, p. ej.: PICO_SDK_PATH=/ruta/a/pico-sdk ./build.sh"
    exit 1
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -DPICO_BOARD=pico -DPICO_SDK_PATH="${PICO_SDK_PATH}" "${BLINK_DIR}"
make -j"$(nproc)"

echo ""
echo "Build complete!"
echo "UF2: ${UF2_FILE}"
echo "ELF: ${ELF_FILE}"