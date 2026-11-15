# compiler-macgcc.mk - Retro68 cross-compiler for the classic 68k Mac
#
# Requires a built Retro68 toolchain:
#   https://github.com/autc04/Retro68
# Add to PATH: export PATH=$RETRO68_BUILD/toolchain/bin:$PATH

CC      = m68k-apple-macos-gcc
AR      = m68k-apple-macos-ar
AS      = m68k-apple-macos-as

# -mcpu=68000: every Mac with HD20 support in ROM (512Ke, Plus, SE) and up
CFLAGS  = -Wall -O2 -std=gnu99 -mcpu=68000 -ffunction-sections -fdata-sections

ASFLAGS =
ARFLAGS = rcs
LIBEXT  = .a
