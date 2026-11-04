#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/list_pages_wire_test"

mkdir -p "$(dirname "$OUT")"
gcc -std=c99 -Wall -Wextra -Werror \
    -I"$ROOT/include" \
    "$ROOT/tests/list_pages_wire_test.c" \
    "$ROOT/src/common/fn_file_list_pages.c" \
    -o "$OUT"
"$OUT"

# Same independent wire vectors through the BBC ROM-call adapter.
gcc -std=c99 -Wall -Wextra -Werror -DTEST_BBC_RAW -D__fastcall__= \
    -I"$ROOT/include" "$ROOT/tests/list_pages_wire_test.c" \
    "$ROOT/src/common/fn_file_list_pages.c" "$ROOT/src/platform/bbc/fn_raw.c" \
    -o "${OUT}_bbc"
"${OUT}_bbc"
