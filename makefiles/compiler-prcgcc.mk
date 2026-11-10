# compiler-prcgcc.mk - prc-tools m68k-palmos-gcc (gcc 2.95.3) for Palm OS
#
# gcc 2.95 takes none of -std=c99, -Wextra or -MF and has no stdint.h or
# string.h; include/palmos/ supplies those two (targets add -Iinclude/palmos).

# Compiler tools
CC      = m68k-palmos-gcc
AR      = m68k-palmos-ar

include makefiles/palmos-sdk.mk
CFLAGS  = -Wall -O2 $(PALMOS_SDK_FLAGS)

# Archiver flags
ARFLAGS = rcs

# Output extension
LIBEXT  = .a
