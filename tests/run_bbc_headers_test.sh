#!/bin/sh
set -eu
mkdir -p build/tests
gcc -std=c99 -Wall -Wextra -Werror -D__fastcall__= -Iinclude \
    tests/bbc_headers_test.c src/platform/bbc/fn_bbc_open_headers.c \
    src/platform/bbc/fn_open_headers.c -o build/tests/bbc-headers
build/tests/bbc-headers
