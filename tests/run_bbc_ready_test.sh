#!/bin/sh
set -eu
mkdir -p build/tests
gcc -std=c99 -Wall -Wextra -Werror -D__fastcall__= -Iinclude \
    tests/bbc_ready_test.c src/platform/bbc/fn_ready.c -o build/tests/bbc-ready
build/tests/bbc-ready
