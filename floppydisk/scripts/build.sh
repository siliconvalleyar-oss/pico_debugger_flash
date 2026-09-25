#!/usr/bin/env bash
# =============================================================================
# build.sh — compila el firmware de floppydisk y lo flashea con picoprobe.
#
# Uso:
#   scripts/build.sh                     # menu interactivo de board + compila
#   scripts/build.sh pico2_w             # compila con BOARD=pico2_w
#   scripts/build.sh pico2_w flash       # compila y flashea (picoprobe / SWD)
#   scripts/build.sh pico flash          # lo mismo para una RP2040
#   scripts/build.sh pico2 picotool      # flashea por USB (bootsel + picotool)
#
# Boards soportadas:
#   pico, pico_w  -> RP2040
#   pico2, pico2_w-> RP2350   (alias aceptado: pico_2w / pico2w)
#
# Flasheo con picoprobe (SWD):
#   Requiere OpenOCD con soporte de RP2350 y un probe con firmware picoprobe
#   conectado en SWD (SWCLK=GP2, SWDIO=GP3 del probe).
#
# Variables de entorno utiles:
#   BOARD=...                      board por defecto (si no se pasa argumento)
#   OPENOCD_INTERFACE=path         interface script de openocd (por defecto
#                                  interface/raspberrypi-swd.cfg)
#   DEBUG=1                        agrega -v a openocd
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"

ACTION="${2:-flash}"     # build | flash (default) | picotool | clean

BOARDS=(pico pico_w pico2 pico2_w)

# --- normaliza el nombre de board ------------------------------------------
normalize_board() {
    local b
    b="$(echo "$1" | tr '[:lower:]' '[:lower:]' | tr ' -' '__')"
    case "$b" in
        pico_2w|pico2w|pico2_w) echo "pico2_w" ;;
        pico_w_|pico_w|pico) echo "pico" ;;
*) echo "$b" ;;
    esac
}

# --- comprueba la board y resuelve el BUILD_DIR ----------------------------
resolve_board() {
    local b="$1" ok=0
    for x in "${BOARDS[@]}"; do [ "$x" = "$b" ] && ok=1; done
    [ "$ok" = "1" ] || {
        echo "ERROR: board '$b' no valida."
        echo "   Opciones: ${BOARDS[*]}"
        exit 1
    }
}

# si no nos pasan board, menu interactivo
if [ -z "${1:-}" ]; then
    if [ -n "${BOARD:-}" ]; then
        BOARD="$(normalize_board "$BOARD")"
    else
        echo "Selecciona la board a compilar:"
        select sel in "${BOARDS[@]}"; do
            [ -n "$sel" ] && break
        done
        BOARD="$sel"
    fi
else
    BOARD="$(normalize_board "$1")"
fi
resolve_board "$BOARD"

BUILD_DIR="$PROJECT_DIR/build_$BOARD"
UF2="$BUILD_DIR/floppydisk.uf2"
ELF="$BUILD_DIR/floppydisk.elf"

banner() { echo -e "\e[1;32m==>\e[0m $*"; }

# --- toolchain ---------------------------------------------------------------
need() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "ERROR: no encuentro '$1'. Instala ${2:-$1}."
        exit 1
    }
}
need cmake
need make

echo "floppydisk | board=$BOARD | build_dir=$BUILD_DIR"
banner "Configurando CMake..."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DPICO_BOARD="$BOARD" ${EXTRA_CMAKE_ARGS:-}

if [ "$ACTION" = "clean" ]; then
    rm -rf "$BUILD_DIR"
    echo "build/$BOARD borrado."
    exit 0
fi

banner "Compilando (release)..."
cmake --build "$BUILD_DIR" --target floppydisk -j"$(nproc)"

[ -f "$UF2" ] || { echo "ERROR: no se genero $UF2"; exit 1; }
echo "OK: $UF2 ($(du -h "$UF2" | cut -f1))"

if [ "$ACTION" = "build" ]; then
    exit 0
fi

# --- picotool (bootsel USB) ---------------------------------------------------
if [ "$ACTION" = "picotool" ]; then
    need picotool "picotool (sudo apt install picotool)"
    banner "Flasheando por USB (bootsel) con picotool..."
    picotool load -x "$UF2"
    picotool reboot -u
    echo "OK: picotool cargo el firmware."
    exit 0
fi

# --- picoprobe / OpenOCD (SWD) ------------------------------------------------
if [ "$ACTION" = "flash" ]; then
    need openocd "openocd con soporte de RP2350 (disponible en pico-probe de Raspberry Pi)"

    case "$BOARD" in
        pico|pico_w)
            TARGET_TXT="rp2040.cfg" ;;
        *)
            # RP2350: el firmware compila por defecto para cores ARM (cortex-m33),
            # pero si se construyo para RISC-V se necesita el target riscv.
            if [ -f "$ELF" ] && readelf -h "$ELF" 2>/dev/null | grep -qi "RISC-V"; then
                TARGET_TXT="rp2350-riscv.cfg"
            else
                TARGET_TXT="rp2350.cfg"
            fi
            ;;
    esac
    INTERFACE="${OPENOCD_INTERFACE:-interface/raspberrypi-swd.cfg}"

    banner "Flasheando con OpenOCD + picoprobe (SWD)..." 
    echo "   interface: $INTERFACE"
    echo "   target:    $TARGET_TXT"

    openocd -f "$INTERFACE" \
            -f "target/$TARGET_TXT" \
            -c "program \"$ELF\" verify reset exit" ${EXTRA_OPENOCD_ARGS:-}

    echo "OK: firmware flasheado via SWD."
    exit 0
fi

echo "ERROR: accion desconocida '$ACTION' (build | flash | picotool | clean)"
exit 1