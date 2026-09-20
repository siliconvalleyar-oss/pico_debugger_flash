#!/bin/bash
# flash_nosudo_multi.sh - Menú interactivo: obtiene la ruta del proyecto a
# COMPILAR y PROGRAMAR. Compila el firmware y lo graba en el Pico target por
# SWD **SIN sudo** (regla udev instalada con scripts/install_udev.sh).
#
# Uso:
#   ./scripts/flash_nosudo_multi.sh            # menú interactivo
#   ./scripts/flash_nosudo_multi.sh /ruta/proyecto   # modo directo (no interactivo)
#
# Variables opcionales:
#   BOARD=pico_w          target Pico W (default: pico)
#   PICO_SDK_PATH=/ruta   ruta al Pico SDK si no está en ../pico-sdk

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

BOARD="${BOARD:-pico}"

# ---------------------------------------------------------------------------
# Detección de proyectos dentro del repositorio (carpetas con CMakeLists.txt)
# ---------------------------------------------------------------------------
mapfile -t AVAILABLE < <(
    for d in "${REPO_ROOT}"/*/; do
        [ -f "${d}CMakeLists.txt" ] && echo "${d%/}"
    done
)

# ---------------------------------------------------------------------------
# Selección de la ruta del proyecto (menú o argumento directo)
# ---------------------------------------------------------------------------
pick_project() {
    local sel i

    echo "=== Pico Debugger Flash - Build & Program (no sudo) ==="
    echo "Board: ${BOARD}    (use BOARD=pico_w para Pico W)"
    echo ""
    print_toolchain
    echo ""

    if [ "${#AVAILABLE[@]}" -gt 0 ]; then
        echo "Proyectos detectados en ${REPO_ROOT}:"
        i=1
        for d in "${AVAILABLE[@]}"; do
            printf "  %2d) %s\n" "$i" "$(basename "${d}")"
            i=$((i + 1))
        done
        echo ""
    else
        echo "No se detectaron proyectos en ${REPO_ROOT}."
    fi

    while true; do
        echo -n "Ingrese el número del proyecto, pegue una ruta, o 'q' para salir: "
        read -r sel
        [ -z "${sel}" ] && continue

        if [ "${sel}" = "q" ] || [ "${sel}" = "Q" ]; then
            echo "Saliendo."
            exit 0
        fi

        if [[ "${sel}" =~ ^[0-9]+$ ]]; then
            i=$((sel))
            if [ "${i}" -ge 1 ] && [ "${i}" -le "${#AVAILABLE[@]}" ]; then
                PROJECT_DIR="${AVAILABLE[$((sel - 1))]}"
            else
                echo "  -> Opción inválida (${sel})."
                continue
            fi
        elif [ -d "${sel}" ]; then
            PROJECT_DIR="$(cd "${sel}" && pwd)"
        else
            echo "  -> La ruta '${sel}' no existe."
            continue
        fi
        break
    done

    echo ""
    echo "Proyecto seleccionado: ${PROJECT_DIR}"
    echo "------------------------------------------------------------"
}

if [ "$#" -ge 1 ]; then
    if [ ! -d "$1" ]; then
        echo "Error: el directorio '$1' no existe" >&2
        exit 1
    fi
    PROJECT_DIR="$(cd "$1" && pwd)"
    echo "=== Pico Debugger Flash - Build & Program (no sudo) ==="
    echo "Board: ${BOARD}    (use BOARD=pico_w para Pico W)"
    echo ""
    print_toolchain
    echo ""
    echo "Proyecto seleccionado: ${PROJECT_DIR}"
    echo "------------------------------------------------------------"
else
    pick_project
fi

PROJECT="$(basename "${PROJECT_DIR}")"
BUILD_DIR="${PROJECT_DIR}/build"
# Los binarios quedan en build/src/ porque el CMake del proyecto hace add_subdirectory(src).
# Use PROJECT_CMAKE_TARGET if set (may differ from directory name).
ELF_FILE="${BUILD_DIR}/src/${PROJECT_CMAKE_TARGET:-${PROJECT}}.elf"
UF2_FILE="${BUILD_DIR}/src/${PROJECT_CMAKE_TARGET:-${PROJECT}}.uf2"

export PROJECT PROJECT_DIR BUILD_DIR ELF_FILE UF2_FILE BOARD

# ---------------------------------------------------------------------------
# Validaciones previas
# ---------------------------------------------------------------------------
if [ ! -f "${PROJECT_DIR}/CMakeLists.txt" ]; then
    echo "Error: ${PROJECT_DIR}/CMakeLists.txt no existe" >&2
    exit 1
fi

if [ ! -r "${CONFIG_FILE}" ]; then
    echo "Error: ${CONFIG_FILE} no existe" >&2
    exit 1
fi

if [ ! -x "${OPENOCD_BIN}" ]; then
    echo "Error: openocd no encontrado (${OPENOCD_BIN})" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Paso 1: compilar
# ---------------------------------------------------------------------------
echo ""
echo "############################################################"
echo "# Paso 1/2 - Compilar ${PROJECT}"
echo "############################################################"
"${SCRIPT_DIR}/build.sh"

if [ ! -f "${ELF_FILE}" ]; then
    echo "Error: Build failed - ${ELF_FILE} no encontrado" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Paso 2: programar SIN sudo vía OpenOCD (Debug Probe / CMSIS-DAP)
# ---------------------------------------------------------------------------
echo ""
echo "############################################################"
echo "# Paso 2/2 - Programar ${PROJECT} (no sudo, SWD)"
echo "############################################################"
echo "ELF: ${ELF_FILE}"
echo ""

run_openocd "${CONFIG_FILE}" -c "program ${ELF_FILE} verify reset exit"

echo ""
echo "=== Programación completa: ${PROJECT} grabado sin sudo ==="