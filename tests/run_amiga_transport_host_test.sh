#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DRIVER_INCLUDE="$ROOT/../fujinet-nio-driver/amiga/include"
OUT="$ROOT/build/tests/amiga_transport_host_test"

mkdir -p "$(dirname "$OUT")"
gcc -std=c99 -Wall -Wextra -Werror \
    -D__AMIGA__ -DFN_AMIGA_EXPLICIT_LIFECYCLE \
    -I"$ROOT/tests/amiga_transport_stubs" \
    -I"$ROOT/include" \
    -I"$DRIVER_INCLUDE" \
    "$ROOT/tests/amiga_transport_host_test.c" \
    "$ROOT/src/platform/amiga/fn_transport.c" \
    "$ROOT/src/common/fn_state.c" \
    -o "$OUT"
"$OUT"
