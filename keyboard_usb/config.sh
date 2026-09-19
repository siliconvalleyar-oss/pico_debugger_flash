#!/bin/bash
# config.sh - Configuración compartida

PROJECT="keyboard_usb"
BOARD="${BOARD:-pico}"
PICO_SDK_PATH="${PICO_SDK_PATH:-/mnt/disk/src/rpico/pico-sdk}"

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
ELF_FILE="${BUILD_DIR}/${PROJECT}.elf"
UF2_FILE="${BUILD_DIR}/${PROJECT}.uf2"

OPENOCD_BIN="/mnt/disk/src/rpico/openocd-src/src/openocd"
OPENOCD_SCRIPTS="/mnt/disk/src/rpico/openocd-src/tcl"
CONFIG_FILE="/mnt/disk/src/rpico/debugprobe-openocd.cfg"
HIDAPI_LIB="/mnt/disk/src/rpico/hidapi-install/lib"

print_toolchain() {
    echo "SDK: ${PICO_SDK_PATH}"
    echo "OpenOCD: ${OPENOCD_BIN}"
    echo "Config: ${CONFIG_FILE}"
    echo "HIDAPI lib: ${HIDAPI_LIB}"
}

run_openocd() {
    local cfg="$1"
    shift
    LD_LIBRARY_PATH="${HIDAPI_LIB}" "${OPENOCD_BIN}" \
        -s "${OPENOCD_SCRIPTS}" \
        -f "${cfg}" \
        "$@"
}