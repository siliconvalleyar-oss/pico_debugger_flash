#!/bin/bash
# serial_debug.sh - Debug interactivo del puerto serie del Pico
# Permite enviar comandos al shell del firmware y ver las respuestas.
#
# Uso:
#   ./scripts/serial_debug.sh           # menú interactivo (puerto por defecto /dev/ttyACM0)
#   ./scripts/serial_debug.sh /dev/ttyACM0  # puerto específico
#   BAUD=115200 ./scripts/serial_debug.sh   # baudrate personalizado
#
# Requiere: stty, timeout, bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

SERIAL_PORT="${1:-/dev/ttyACM0}"
BAUD="${BAUD:-115200}"
TIMEOUT="${TIMEOUT:-3}"

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Comandos predefinidos del shell floppydisk_usb
declare -A COMMANDS=(
    ["1"]="help|Muestra ayuda de comandos"
    ["2"]="sdtest|Test completo microSD (CMD0, CMD8, ACMD41, CMD58, CMD16, CMD9, CMD17)"
    ["3"]="fat|Info FAT (libre/total KB)"
    ["4"]="ls|Lista imágenes en /IMG"
    ["5"]="version|Versión firmware y commit git"
    ["6"]="reset|Reinicia el Pico"
    ["7"]="echo on|Activa eco de comandos"
    ["8"]="echo off|Desactiva eco de comandos"
)

print_header() {
    clear
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  Pico Serial Debug - floppydisk_usb   ${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "Puerto: ${GREEN}${SERIAL_PORT}${NC}  |  Baud: ${GREEN}${BAUD}${NC}  |  Timeout: ${GREEN}${TIMEOUT}s${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

check_port() {
    if [ ! -e "${SERIAL_PORT}" ]; then
        echo -e "${RED}Error: Puerto ${SERIAL_PORT} no existe${NC}"
        echo "Puertos disponibles:"
        ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "  (ninguno)"
        exit 1
    fi
    if [ ! -r "${SERIAL_PORT}" ] || [ ! -w "${SERIAL_PORT}" ]; then
        echo -e "${RED}Error: Sin permisos en ${SERIAL_PORT}${NC}"
        echo "Ejecute: sudo usermod -a -G dialout \$USER && newgrp dialout"
        exit 1
    fi
}

configure_port() {
    stty -F "${SERIAL_PORT}" "${BAUD}" raw -echo -echoe -echok -echonl -icanon -isig -ixon -ixoff -clocal -crtscts 2>/dev/null
    stty -F "${SERIAL_PORT}" min 0 time 1 2>/dev/null
}

send_command() {
    local cmd="$1"
    local wait_time="${2:-0.5}"
    
    # Enviar comando + newline
    printf "%s\r\n" "${cmd}" > "${SERIAL_PORT}"
    sleep "${wait_time}"
    
    # Leer respuesta
    timeout "${TIMEOUT}" cat "${SERIAL_PORT}" 2>/dev/null || true
}

print_menu() {
    echo -e "${CYAN}Comandos predefinidos:${NC}"
    for i in 1 2 3 4 5 6 7 8; do
        IFS='|' read -r cmd desc <<< "${COMMANDS[$i]}"
        printf "  ${YELLOW}%s${NC}) ${GREEN}%-12s${NC} - %s\n" "$i" "$cmd" "$desc"
    done
    echo ""
    echo -e "  ${YELLOW}c${NC}) Comando personalizado"
    echo -e "  ${YELLOW}m${NC}) Monitor continuo (Ctrl+C para salir)"
    echo -e "  ${YELLOW}r${NC}) Reconectar puerto"
    echo -e "  ${YELLOW}q${NC}) Salir"
    echo ""
}

interactive_loop() {
    local choice cmd
    
    while true; do
        print_header
        print_menu
        echo -n -e "${CYAN}Seleccione opción: ${NC}"
        read -r choice
        
        case "${choice}" in
            1|2|3|4|5|6|7|8)
                IFS='|' read -r cmd desc <<< "${COMMANDS[$choice]}"
                echo -e "${BLUE}>>> Enviando: ${cmd}${NC}"
                send_command "${cmd}"
                echo ""
                echo -n "Presione Enter para continuar..."
                read -r
                ;;
            c|C)
                echo -n -e "${CYAN}Comando: ${NC}"
                read -r cmd
                [ -n "${cmd}" ] && send_command "${cmd}"
                echo ""
                echo -n "Presione Enter para continuar..."
                read -r
                ;;
            m|M)
                echo -e "${YELLOW}Monitor continuo - Ctrl+C para salir${NC}"
                echo -e "${BLUE}>>> Conectado a ${SERIAL_PORT} @ ${BAUD}${NC}"
                echo ""
                # Configurar trap para limpiar al salir
                trap 'echo -e "\n${BLUE}>>> Desconectado${NC}"; stty sane; exit 0' INT TERM
                cat "${SERIAL_PORT}"
                ;;
            r|R)
                echo -e "${YELLOW}Reconectando...${NC}"
                sleep 1
                configure_port
                echo -e "${GREEN}OK${NC}"
                sleep 0.5
                ;;
            q|Q)
                echo -e "${GREEN}Saliendo...${NC}"
                break
                ;;
            *)
                echo -e "${RED}Opción inválida${NC}"
                sleep 0.5
                ;;
        esac
    done
}

# Modo no interactivo: enviar un comando y salir
if [ $# -ge 2 ]; then
    check_port
    configure_port
    shift
    send_command "$*"
    exit 0
fi

# Modo interactivo
check_port
configure_port
echo -e "${GREEN}Puerto configurado: ${SERIAL_PORT} @ ${BAUD}${NC}"
sleep 0.5

# Enviar Enter inicial para ver prompt
send_command "" 0.2

interactive_loop

stty sane
echo -e "${GREEN}Desconectado${NC}"