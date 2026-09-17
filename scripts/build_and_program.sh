#!/bin/bash
# build_and_program.sh - Build + program en un solo paso (usa sudo si hace falta).

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Building and Programming Blink Firmware ==="
echo ""

"${SCRIPT_DIR}/build.sh"
echo ""
"${SCRIPT_DIR}/program.sh"

echo ""
echo "=== Done ==="