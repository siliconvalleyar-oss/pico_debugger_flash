#!/bin/bash
# scripts/test_serial.sh - Test commands over USB CDC serial
# Usage: scripts/test_serial.sh [device] [command]
# Example: scripts/test_serial.sh /dev/ttyACM1 ls
# Example: scripts/test_serial.sh /dev/ttyACM1 sdtest
# Example: scripts/test_serial.sh /dev/ttyACM1 version

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

DEVICE="${1:-/dev/ttyACM1}"
COMMAND="${2:-help}"
BAUD="${BAUD:-115200}"
TIMEOUT="${TIMEOUT:-5}"

if [ ! -e "${DEVICE}" ]; then
    echo "Error: Device ${DEVICE} does not exist" >&2
    echo "Available ttyACM devices:" >&2
    ls -la /dev/ttyACM* 2>/dev/null || echo "  (none)" >&2
    exit 1
fi

if [ ! -r "${DEVICE}" ] || [ ! -w "${DEVICE}" ]; then
    echo "Error: No permission for ${DEVICE}" >&2
    echo "Try: sudo usermod -a -G dialout \$USER && newgrp dialout" >&2
    exit 1
fi

stty -F "${DEVICE}" "${BAUD}" raw -echo
exec 3<> "${DEVICE}"

# Send command with CR
echo -e "${COMMAND}\r" >&3

# Wait for response to be ready
sleep 1

# Read response with timeout - output directly to stdout
timeout "${TIMEOUT}" cat <&3 &
CAT_PID=$!
sleep 4
kill "${CAT_PID}" 2>/dev/null || true
wait "${CAT_PID}" 2>/dev/null || true

exec 3>&-