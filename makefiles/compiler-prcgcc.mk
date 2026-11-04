# compiler-prcgcc.mk - prc-tools m68k-palmos-gcc (gcc 2.95.3) for Palm OS
#
# gcc 2.95 takes none of -std=c99, -Wextra or -MF and has no stdint.h or
# string.h; include/palmos/ supplies those two (targets add -Iinclude/palmos).

# Compiler tools
CC      = m68k-palmos-gcc
AR      = m68k-palmos-ar

# -palmos3.5 puts the SDK headers (PalmOS.h) on the include path
CFLAGS  = -Wall -O2 -palmos3.5

# Archiver flags
ARFLAGS = rcs

# Output extension
LIBEXT  = .a
