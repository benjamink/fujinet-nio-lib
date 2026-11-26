#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/mac68k_transport_host_test"

mkdir -p "$(dirname "$OUT")"
gcc -std=c99 -Wall -Wextra -Werror \
    -I"$ROOT/tests/mac68k_transport_stubs" \
    -I"$ROOT/include" \
    "$ROOT/tests/mac68k_transport_host_test.c" \
    "$ROOT/src/platform/mac68k/fn_transport.c" \
    "$ROOT/src/common/fn_state.c" \
    -o "$OUT"
"$OUT"
