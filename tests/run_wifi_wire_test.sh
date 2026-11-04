#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/wifi_wire_test"

mkdir -p "$(dirname "$OUT")"
gcc -std=c99 -Wall -Wextra -Werror \
    -I"$ROOT/include" \
    "$ROOT/tests/wifi_wire_test.c" \
    "$ROOT/src/common/fn_wifi.c" \
    "$ROOT/src/common/fn_wifi_set.c" \
    "$ROOT/src/common/fn_wifi_adapter.c" \
    -o "$OUT"
"$OUT"

# The same vectors exercise the BBC implementation through its ROM-call seam.
gcc -std=c99 -Wall -Wextra -Werror -D__fastcall__= \
    -I"$ROOT/include" "$ROOT/tests/wifi_wire_test.c" \
    "$ROOT/tests/bbc_wifi_wire_adapter.c" "$ROOT"/src/platform/bbc/fn_wifi*.c \
    -o "${OUT}_bbc"
"${OUT}_bbc"
