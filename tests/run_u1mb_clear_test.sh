#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/tests
cc -std=c99 -Wall -Wextra -Werror -D__ATARI__ -DFN_U1MB_TEST -Iinclude \
  tests/u1mb_clear_test.c src/platform/atari/fn_u1mb_clear.c -o build/tests/u1mb_clear_test
build/tests/u1mb_clear_test
