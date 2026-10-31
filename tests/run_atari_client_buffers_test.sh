#!/usr/bin/env bash
set -euo pipefail
out=build/tests/atari-client-buffers
mkdir -p "$out"
make -f makefiles/build.mk TARGET=atari PROGRAM=fujinet-nio ATARI_BACKEND=compact lib
soft65c02_unit -b "$out" -i tests/atari_client_buffers_soft65c02.yaml
for mode in default client; do
    flags=()
    if [ "$mode" = client ]; then flags=(-DCLIENT_BUFFER_OWNED); fi
    cl65 -t atari -I include "${flags[@]}" -c -o "$out/$mode.o" tests/atari_client_buffers_link.c
    cl65 -t atari -m "$out/$mode.map" -o "$out/$mode.xex" "$out/$mode.o" build/fujinet-nio-atari-compact.lib
done
if grep -q 'fn_compact_buffers.o):' "$out/client.map"; then
    echo 'default buffers linked despite application-provided buffers' >&2
    exit 1
fi
if ! grep -q 'fn_compact_buffers.o):' "$out/default.map"; then
    echo 'default buffers missing when application did not provide them' >&2
    exit 1
fi
echo 'Atari default and application-owned buffer archive links passed'
