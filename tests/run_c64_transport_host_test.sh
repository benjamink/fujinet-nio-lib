#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/c64_transport_host_test"

mkdir -p "$(dirname "$OUT")"
gcc -std=c99 -Wall -Wextra -Werror \
    -I"$ROOT/tests/c64_transport_stubs" \
    -I"$ROOT/include" \
    "$ROOT/tests/c64_transport_host_test.c" \
    "$ROOT/src/platform/c64/fn_transport.c" \
    "$ROOT/src/common/fn_state.c" \
    -o "$OUT"
"$OUT"
