#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/legacy_appkey_wire_test"

mkdir -p "$(dirname "$OUT")"
gcc -std=c99 -Wall -Wextra -Werror \
    -I"$ROOT/include" \
    -I"$ROOT/src/legacy" \
    "$ROOT/tests/legacy_appkey_wire_test.c" \
    "$ROOT/src/legacy/fn_legacy_appkey_state.c" \
    "$ROOT/src/legacy/fn_legacy_appkey_buffer.c" \
    "$ROOT/src/legacy/fn_legacy_appkey_util.c" \
    "$ROOT/src/legacy/fn_legacy_appkey_uri.c" \
    "$ROOT/src/legacy/fn_legacy_appkey_mkdir.c" \
    "$ROOT/src/legacy/fn_legacy_appkey_set.c" \
    "$ROOT/src/legacy/fn_legacy_appkey_read.c" \
    "$ROOT/src/legacy/fn_legacy_appkey_read_buffered.c" \
    "$ROOT/src/legacy/fn_legacy_appkey_write.c" \
    -o "$OUT"
"$OUT"
