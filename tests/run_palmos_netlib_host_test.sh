#!/bin/sh
# Host test of the Palm OS Network transport's NIO core (src/platform/palmos/shared/netlib) over BSD sockets and an
# in-process server on 127.0.0.1. Address and undefined-behaviour sanitizers are used when the compiler has them.
set -eu

CORE=src/platform/palmos/shared/netlib
mkdir -p build/tests
SAN=""
probe=build/tests/palmos_netlib_san_probe
printf 'int main(void){return 0;}\n' > "$probe.c"
if cc -fsanitize=address,undefined "$probe.c" -o "$probe" >/dev/null 2>&1 && "$probe" >/dev/null 2>&1; then
    SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
fi
# shellcheck disable=SC2086
cc -std=c99 -D_POSIX_C_SOURCE=200809L -g -Wall -Werror $SAN -I"$CORE" \
    tests/palmos_netlib_host_test.c \
    tests/palmos_netlib_stubs/nio_sock_posix.c \
    "$CORE/nio_core.c" "$CORE/nio_net.c" "$CORE/nio_fuji.c" \
    -lpthread -o build/tests/palmos_netlib_host_test
build/tests/palmos_netlib_host_test
