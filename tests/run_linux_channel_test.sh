#!/bin/sh
set -eu
mkdir -p build/tests
gcc -std=c99 -Wall -Wextra -Werror -DFN_CHANNEL_BULK_WRITE -Iinclude \
    tests/linux_channel_test.c src/common/fn_slip.c \
    -Wl,--wrap=write,--wrap=select,--wrap=clock_gettime \
    -o build/tests/linux_channel_test
build/tests/linux_channel_test
