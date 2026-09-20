#!/bin/bash
# config.sh - Configuración compartida para el proyecto pico_debugger_flash.
#
# Portable: todas las rutas se resuelven desde la ubicación de este archivo y
# pueden sobre-escribirse con variables de entorno, p. ej.:
#   OPENOCD_BIN=/ruta/openocd     OPENOCD_SCRIPTS=/ruta/tcl
#   PICO_SDK_PATH=/ruta/pico-sdk  HIDAPI_LIB=/ruta/lib
#
# Uso desde cualquier script:
#   source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/config.sh"

set -euo pipefail

# Raíz del repositorio (un nivel arriba de scripts/)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"

# Directorios clave del proyecto.
# Se elige el firmware con PROJECT (default: pico_usb_drive_configurable).
# En esta rama (pendrive_config_oled) la carpeta del proyecto es
# usb_oled_drive_configurable, pero el target CMake sigue siendo
# pico_usb_drive_configurable (project() en su CMakeLists.txt).
# Se elige el firmware con PROJECT. El nombre de la CARPETA del proyecto
# puede diferir del nombre del TARGET CMake: usb_oled_drive_configurable/
# contiene el proyecto OLED cuyo CMake crea "pico_usb_drive_configurable".
PROJECT="${PROJECT:-keyboard}"
if [ -d "${REPO_ROOT}/keyboard" ]; then
    PROJECT_DIR="${PROJECT_DIR:-${REPO_ROOT}/keyboard}"
    PROJECT_CMAKE_TARGET="pico_keyboard_bridge"
else
    PROJECT_DIR="${PROJECT_DIR:-${REPO_ROOT}/${PROJECT}}"
    PROJECT_CMAKE_TARGET="${PROJECT}"
fi
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build}"
# Los binarios quedan en build/src/ porque el CMake del proyecto hace add_subdirectory(src).
ELF_FILE="${ELF_FILE:-${BUILD_DIR}/src/${PROJECT_CMAKE_TARGET}.elf}"
UF2_FILE="${UF2_FILE:-${BUILD_DIR}/src/${PROJECT_CMAKE_TARGET}.uf2}"

# Configuraciones de OpenOCD incluidas en el repo
CONFIG_FILE="${CONFIG_FILE:-${REPO_ROOT}/debugprobe-openocd.cfg}"
CONFIG_RESCUE_FILE="${CONFIG_RESCUE_FILE:-${REPO_ROOT}/debugprobe-openocd-rescue.cfg}"

# --- Pico SDK -------------------------------------------------------------
# Por defecto busca ../pico-sdk relativo al repo (layout de la máquina de
# desarrollo). Sobre-escribible con PICO_SDK_PATH.
if [ -z "${PICO_SDK_PATH:-}" ]; then
    if [ -d "${REPO_ROOT}/../pico-sdk" ]; then
        PICO_SDK_PATH="${REPO_ROOT}/../pico-sdk"
    else
        PICO_SDK_PATH=""
    fi
fi

# --- OpenOCD --------------------------------------------------------------
if [ -z "${OPENOCD_BIN:-}" ]; then
    if command -v openocd >/dev/null 2>&1; then
        OPENOCD_BIN="$(command -v openocd)"
    elif [ -x "${REPO_ROOT}/../openocd-src/src/openocd" ]; then
        OPENOCD_BIN="${REPO_ROOT}/../openocd-src/src/openocd"
    else
        OPENOCD_BIN="openocd"
    fi
fi

# Directorio de scripts TCL de OpenOCD (arg -s)
if [ -z "${OPENOCD_SCRIPTS:-}" ]; then
    OPENOCD_SCRIPTS=""
    for d in \
        "${REPO_ROOT}/../openocd-src/tcl" \
        "$(dirname "$(readlink -f "$OPENOCD_BIN")")/../share/openocd/scripts" \
        "$(dirname "$(readlink -f "$OPENOCD_BIN")")/tcl" \
        "/usr/share/openocd/scripts" \
        "/usr/local/share/openocd/scripts"; do
        if [ -n "$d" ] && [ -d "$d" ]; then
            OPENOCD_SCRIPTS="$d"
            break
        fi
    done
fi

# --- hidapi (requerida por el driver CMSIS-DAP) ---------------------------
if [ -z "${HIDAPI_LIB:-}" ]; then
    HIDAPI_LIB=""
    for d in \
        "${REPO_ROOT}/../hidapi-install/lib" \
        "/usr/local/lib" \
        "/usr/lib/x86_64-linux-gnu"; do
        if [ -n "$d" ] && [ -d "$d" ] && ls "$d"/libhidapi-hidraw.so* >/dev/null 2>&1; then
            HIDAPI_LIB="$d"
            break
        fi
    done
fi

# Ejecutar OpenOCD con la ruta de libhidapi ya incorporada.
# Uso: run_openocd <config.cfg> [args...]
run_openocd() {
    local cfg="${1:-}"
    shift || true
    local lib_path="${HIDAPI_LIB}"
    if [ -n "${LD_LIBRARY_PATH:-}" ]; then
        lib_path="${HIDAPI_LIB}:${LD_LIBRARY_PATH}"
    fi
    LD_LIBRARY_PATH="${lib_path}" \
        "${OPENOCD_BIN}" -s "${OPENOCD_SCRIPTS}" -f "${cfg}" "$@"
}

print_toolchain() {
    echo "repo          : ${REPO_ROOT}"
    echo "pico-sdk      : ${PICO_SDK_PATH:-<no definido>}"
    echo "openocd       : ${OPENOCD_BIN}  (scripts: ${OPENOCD_SCRIPTS:-<no hallado>})"
    echo "hidapi libs   : ${HIDAPI_LIB:-<no hallado>}"
    echo "config swd    : ${CONFIG_FILE}"
    echo "config rescue : ${CONFIG_RESCUE_FILE}"
}