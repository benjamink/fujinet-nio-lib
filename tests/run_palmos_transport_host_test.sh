#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Same default as makefiles/targets.mk: the CS1 ABI header lives in fujinet-nio.
SBNIO_ABI_INCLUDE="${SBNIO_ABI_INCLUDE:--I$ROOT/../fujinet-nio/bridges/rp2350-springboard/include}"
OUT="$ROOT/build/tests/palmos_transport_host_test"

if ! printf '#include "sbnio.h"\n' | gcc -E -x c $SBNIO_ABI_INCLUDE - >/dev/null 2>&1; then
    echo "sbnio.h not found; set SBNIO_ABI_INCLUDE=-I<fujinet-nio>/bridges/rp2350-springboard/include" >&2
    exit 1
fi

# The Springboard client: everything in src/platform/palmos but the cradle
# serial channel, which is the palmos-serial target's.
SOURCES=()
for f in "$ROOT"/src/platform/palmos/*.c; do
    [ "$(basename "$f")" = fn_channel_serial.c ] || SOURCES+=("$f")
done

mkdir -p "$(dirname "$OUT")"
gcc -std=c99 -Wall -Wextra -Werror \
    -D__palmos__ -DFN_PALMOS_HOST_TEST \
    -I"$ROOT/tests/palmos_transport_stubs" \
    -I"$ROOT/include" \
    $SBNIO_ABI_INCLUDE \
    "$ROOT/tests/palmos_transport_host_test.c" \
    "${SOURCES[@]}" \
    "$ROOT/src/common/fn_state.c" \
    "$ROOT/src/common/fn_init.c" \
    "$ROOT/src/common/fn_shutdown.c" \
    "$ROOT/src/common/fn_raw.c" \
    "$ROOT/src/common/fn_packet_header.c" \
    "$ROOT/src/common/fn_packet_parse_common.c" \
    "$ROOT/src/common/fn_packet_checksum.c" \
    "$ROOT/src/common/fn_packet_checksum_packet.c" \
    "$ROOT/src/common/fn_checksum_fold.c" \
    -o "$OUT"
"$OUT"
