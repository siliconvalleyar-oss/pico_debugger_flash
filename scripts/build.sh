#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

BOARD="${BOARD:-pico}"

echo "Building ${PROJECT} firmware for board '${BOARD}' (RP2040)..."
echo "Tip: BOARD=pico_w para Pico W; BOARD=pico (default) para Pico 1."
echo "Build: ${BUILD_DIR}"
echo "SDK:   ${PICO_SDK_PATH:-<no definido>}"

if [ -z "${PICO_SDK_PATH:-}" ]; then
    echo "Error: PICO_SDK_PATH no está definido y no se encontró ../pico-sdk"
    echo "Exporte la variable, p. ej.: PICO_SDK_PATH=/ruta/a/pico-sdk BOARD=pico_w ./scripts/build.sh"
    exit 1
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -DPICO_BOARD="${BOARD}" -DPICO_SDK_PATH="${PICO_SDK_PATH}" "${PROJECT_DIR}"
make -j"$(nproc)"

echo ""
echo "Build complete!"
echo "UF2: ${UF2_FILE}"
echo "ELF: ${ELF_FILE}"
