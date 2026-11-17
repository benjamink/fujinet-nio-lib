# compiler-gcc.mk - GCC Compiler Configuration for Linux/POSIX
#
# Native Linux build using GCC

# Compiler tools
# Use absolute paths to avoid conflicts with cc65 tools (ar65, etc.)
CC = /usr/bin/gcc
AR = /usr/bin/ar
AS = /usr/bin/as

# Platform name (used for source directory)
PLATFORM = linux

# Compiler flags
CFLAGS = -Wall -Wextra -O2 -std=c99 -DFN_CHANNEL_BULK_WRITE

# Assembler flags (not typically used for Linux target)
ASFLAGS =

# Archiver flags
ARFLAGS = rcs

# Output extension
LIBEXT = .a
