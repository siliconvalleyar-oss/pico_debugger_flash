#!/bin/bash
# build.sh - Compila el firmware

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

echo "=== ${PROJECT} Firmware - Build ==="
echo "Board: ${BOARD}"
echo "Project: ${PROJECT_DIR}"
echo "Build: ${BUILD_DIR}"
echo "SDK: ${PICO_SDK_PATH}"
echo ""

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -DPICO_BOARD="${BOARD}" -DPICO_SDK_PATH="${PICO_SDK_PATH}" ..
make -j$(nproc)

echo ""
echo "Build complete!"
echo "UF2: ${UF2_FILE}"
echo "ELF: ${ELF_FILE}"