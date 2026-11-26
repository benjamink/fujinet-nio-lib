#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/clock_format_wire_test"
mkdir -p "$(dirname "$OUT")"
gcc -std=c89 -pedantic -Wall -Wextra -Werror -I"$ROOT/include" \
    "$ROOT/tests/clock_format_wire_test.c" \
    "$ROOT/src/common/fn_clock_format_capacity.c" \
    "$ROOT/src/common/fn_clock_format_common.c" \
    "$ROOT/src/common/fn_clock_get_format.c" \
    "$ROOT/src/common/fn_clock_get_tz.c" -o "$OUT"
"$OUT"
