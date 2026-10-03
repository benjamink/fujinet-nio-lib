#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/open_wire_test"

mkdir -p "$(dirname "$OUT")"
gcc -std=c99 -Wall -Wextra -Werror \
    -I"$ROOT/include" \
    "$ROOT/tests/open_wire_test.c" \
    "$ROOT/src/common/fn_open.c" \
    "$ROOT/src/common/fn_open_translated.c" \
    "$ROOT/src/common/fn_state.c" \
    "$ROOT/src/common/fn_packet_parse_open.c" \
    "$ROOT/src/common/fn_packet_parse_common.c" \
    "$ROOT/src/common/fn_packet_build_open.c" \
    "$ROOT/src/common/fn_packet_build_open_ext.c" \
    "$ROOT/src/common/fn_packet_header.c" \
    "$ROOT/src/common/fn_packet_checksum_packet.c" \
    "$ROOT/src/common/fn_packet_checksum.c" \
    "$ROOT/src/common/fn_checksum_fold.c" \
    -o "$OUT"
"$OUT"
