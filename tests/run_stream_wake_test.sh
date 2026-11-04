#!/bin/sh
set -eu

mkdir -p build/tests
gcc -std=c99 -Wall -Wextra -Werror -Iinclude \
    -DFN_STREAM_WAKE_IDLE_MS=30000 -DFN_STREAM_WAKE_WAIT_MS=20 -DFN_STREAM_WAKE_ATTEMPTS=8 \
    tests/stream_wake_test.c \
    src/common/fn_transport_stream.c \
    src/common/fn_session.c \
    src/common/fn_slip.c \
    src/common/fn_packet_header.c \
    src/common/fn_packet_checksum_packet.c \
    src/common/fn_packet_checksum.c \
    src/common/fn_checksum_fold.c \
    -o build/tests/stream_wake_test
build/tests/stream_wake_test
