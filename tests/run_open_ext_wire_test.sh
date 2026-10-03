#!/bin/sh
# tests/run_open_ext_wire_test.sh
set -eu
mkdir -p build/tests
gcc -std=c99 -Wall -Wextra -Werror -Iinclude \
    tests/open_ext_wire_test.c \
    src/common/fn_packet_build_open.c \
    src/common/fn_packet_build_open_ext.c \
    src/common/fn_packet_header.c \
    src/common/fn_packet_checksum_packet.c \
    src/common/fn_packet_checksum.c \
    src/common/fn_checksum_fold.c \
    -o build/tests/open_ext_wire_test
build/tests/open_ext_wire_test
