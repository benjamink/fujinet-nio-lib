#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/file_wire_test"
mkdir -p "$(dirname "$OUT")"
for packet_size in 512 1024; do
gcc -std=c89 -pedantic -Wall -Wextra -Werror -I"$ROOT/include" \
    -DFN_MAX_PACKET_SIZE="$packet_size" \
    "$ROOT/tests/file_wire_test.c" \
    "$ROOT/src/common/fn_file_common.c" \
    "$ROOT/src/common/fn_file_stat.c" \
    "$ROOT/src/common/fn_file_read.c" \
    "$ROOT/src/common/fn_file_write.c" \
    "$ROOT/src/common/fn_file_mkdir.c" \
    "$ROOT/src/common/fn_disk_inspect.c" \
    "$ROOT/src/common/fn_disk_common.c" -o "$OUT-$packet_size"
"$OUT-$packet_size"
done
