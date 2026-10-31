#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/tests
cc -std=c99 -Wall -Wextra -Werror -Iinclude tests/fuji_hsio_test.c \
  src/common/fn_fuji_hsio_call.c src/common/fn_fuji_hsio_get.c src/common/fn_fuji_hsio_set.c \
  -o build/tests/fuji_hsio_test
build/tests/fuji_hsio_test
