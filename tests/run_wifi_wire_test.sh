#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/wifi_wire_test"

mkdir -p "$(dirname "$OUT")"
gcc -std=c99 -Wall -Wextra -Werror \
    -I"$ROOT/include" \
    "$ROOT/tests/wifi_wire_test.c" \
    "$ROOT/src/common/fn_wifi.c" \
    "$ROOT/src/common/fn_wifi_adapter.c" \
    -o "$OUT"
"$OUT"
