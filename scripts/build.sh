#!/bin/bash
# scripts/build.sh - Compila el firmware usando la configuracion ya heredada
# de scripts/config.sh (que TODO flash_nosu* hereda). Este era el fichero que
# faltaba en la cadena: flash_nosudo_multi.sh:135 lo invoca y no existia en el
# repo (leccion cerrada en esta sesion: sin build.sh no hay ninja, sin ninja
# [100%] no hay ELF, sin ELF no se flashea - y se dice).
#
# Uso:
#   scripts/build.sh                      # usa PROJECT/PROJECT_DIR/... de config.sh
#   PROJECT=keyboard scripts/build.sh     # puente BLE doorbell + USB HID keyboard

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

echo "=== Building ${PROJECT} ==="
echo "Project dir : ${PROJECT_DIR}"
echo "CMake target: ${PROJECT_CMAKE_TARGET:-${PROJECT}}"
echo "Board       : ${BOARD:-pico_w}"

# Ensure we have the latest code before building
cd "${REPO_ROOT}" && git pull --ff-only

# Clean build directory to avoid generator mismatch (Ninja vs Makefiles)
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Use default generator (Unix Makefiles) for consistency
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    -DPICO_SDK_PATH="${PICO_SDK_PATH:-${PICO_SDK_PATH-}}" \
    -DPICO_BOARD="${BOARD:-pico_w}" \
    ${TOOLCHAIN_DEFS:-}

cmake --build "${BUILD_DIR}"

echo ""
echo "=== Build done: ${BUILD_DIR} ==="
