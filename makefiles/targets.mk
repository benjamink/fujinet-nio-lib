# targets.mk - Target Platform Definitions
#
# Maps build targets to platforms and compiler families.

# Platform mapping
# Each target maps to a platform directory under src/platform/

PLATFORM_atari      := atari
PLATFORM_apple2     := apple2
PLATFORM_apple2enh  := apple2
PLATFORM_bbc        := bbc
PLATFORM_bbc-clib   := bbc
PLATFORM_c64        := c64
PLATFORM_coco       := coco
PLATFORM_msdos      := msdos
PLATFORM_msdos-serial := msdos
PLATFORM_msdos-ioctl := msdos
PLATFORM_msdos-f5 := msdos
PLATFORM_linux      := linux
PLATFORM_amiga      := amiga
PLATFORM_amiga-driver := amiga
PLATFORM_palmos-serial := palmos
PLATFORM_palmos-springboard := palmos

# Get platform for current target
PLATFORM := $(PLATFORM_$(TARGET))

# Compiler family mapping
# Determines which compiler configuration to use

COMPILER_FAMILY_atari      := cc65
COMPILER_FAMILY_apple2     := cc65
COMPILER_FAMILY_apple2enh  := cc65
COMPILER_FAMILY_bbc        := cc65
COMPILER_FAMILY_bbc-clib   := cc65
COMPILER_FAMILY_c64        := cc65
COMPILER_FAMILY_coco       := cmoc
COMPILER_FAMILY_msdos      := wcc
COMPILER_FAMILY_msdos-serial := wcc
COMPILER_FAMILY_msdos-ioctl := wcc
COMPILER_FAMILY_msdos-f5 := wcc
COMPILER_FAMILY_linux      := gcc
COMPILER_FAMILY_amiga      := amigagcc
COMPILER_FAMILY_amiga-driver := amigagcc
COMPILER_FAMILY_palmos-serial := prcgcc
COMPILER_FAMILY_palmos-springboard := prcgcc

# Get compiler family for current target
COMPILER_FAMILY := $(COMPILER_FAMILY_$(TARGET))

# The cc65 target a build compiles for, normally the target itself. The C64
# compiles with no target: cc65's c64 target translates every string literal to
# PETSCII, but everything crossing FujiBus (URIs, keys, host names) is ASCII. Its
# programs keep their strings in ASCII and convert only at the screen and the
# keyboard. It still links against c64.lib; TARGET_CFLAGS_c64 restores the macros
# the target would have defined.
CC65_TARGET_c64 := none
CC65_TARGET := $(if $(CC65_TARGET_$(TARGET)),$(CC65_TARGET_$(TARGET)),$(TARGET))

# Transport family mapping
# stream targets provide only a byte-channel implementation and share the common
# SLIP/FujiBus transport in src/common/fn_transport_stream.c.
TRANSPORT_FAMILY_msdos     := stream
TRANSPORT_FAMILY_msdos-serial := stream
TRANSPORT_FAMILY_linux      := stream
TRANSPORT_FAMILY_palmos-serial := stream

# Get transport family for current target
TRANSPORT_FAMILY := $(TRANSPORT_FAMILY_$(TARGET))

# Target-specific flags

# The session pool is part of the library's platform configuration.  Keep the
# value in the target build and pass it to both C and assembly sources so the
# C allocation and the ca65 session loops cannot drift apart.
FN_MAX_SESSIONS_bbc      := 2
FN_MAX_SESSIONS_bbc-clib := $(FN_MAX_SESSIONS_bbc)

# Atari
TARGET_CFLAGS_atari     :=
TARGET_ASFLAGS_atari    :=

# Apple II
TARGET_CFLAGS_apple2    :=
TARGET_ASFLAGS_apple2   :=

# Apple II Enhanced
TARGET_CFLAGS_apple2enh :=
TARGET_ASFLAGS_apple2enh :=

# BBC Micro
TARGET_CFLAGS_bbc      := -DFN_MAX_SESSIONS=$(FN_MAX_SESSIONS_bbc)
TARGET_ASFLAGS_bbc     := --asm-define FN_MAX_SESSIONS=$(FN_MAX_SESSIONS_bbc)

# BBC Micro CLIB target
TARGET_CFLAGS_bbc-clib  := -DFN_MAX_SESSIONS=$(FN_MAX_SESSIONS_bbc-clib)
TARGET_ASFLAGS_bbc-clib := --asm-define FN_MAX_SESSIONS=$(FN_MAX_SESSIONS_bbc-clib)

# Commodore 64
TARGET_CFLAGS_c64       := -D__C64__=1 -D__CBM__=1
TARGET_ASFLAGS_c64      :=

# CoCo
TARGET_CFLAGS_coco      :=
TARGET_ASFLAGS_coco     :=

# MS-DOS
TARGET_CFLAGS_msdos     :=
TARGET_ASFLAGS_msdos    :=
TARGET_CFLAGS_msdos-serial :=
TARGET_ASFLAGS_msdos-serial :=
TARGET_CFLAGS_msdos-ioctl :=
TARGET_ASFLAGS_msdos-ioctl :=
TARGET_CFLAGS_msdos-f5 :=
TARGET_ASFLAGS_msdos-f5 :=

TARGET_PLATFORM_SRCS_msdos := src/platform/msdos/fn_channel_serial.c
TARGET_PLATFORM_SRCS_msdos-serial := src/platform/msdos/fn_channel_serial.c
TARGET_PLATFORM_SRCS_msdos-ioctl := src/platform/msdos/fn_transport_ioctl.c
TARGET_PLATFORM_SRCS_msdos-f5 := src/platform/msdos/fn_transport_f5.c

# Linux (native)
TARGET_CFLAGS_linux     :=
TARGET_ASFLAGS_linux    :=

# Amiga (m68k-amigaos cross-compile via amiga-gcc)
# Broker ABI lives in the driver tree; do not vendor a second copy.
AMIGA_NIO_DEVICE_INCLUDE := -I../fujinet-nio-driver/amiga/include
TARGET_CFLAGS_amiga     := -mcpu=68000 -msoft-float $(AMIGA_NIO_DEVICE_INCLUDE)
TARGET_ASFLAGS_amiga    :=

# Resident Amiga drivers cannot depend on process-exit startup/teardown.
# They own transport shutdown as part of their device lifecycle instead.
TARGET_CFLAGS_amiga-driver := -mcpu=68000 -msoft-float -DFN_AMIGA_EXPLICIT_LIFECYCLE $(AMIGA_NIO_DEVICE_INCLUDE)
TARGET_ASFLAGS_amiga-driver :=

# Palm OS over the cradle serial port (prc-tools). NIO may lend an idle cradle
# line to HotSync between requests, so the stream transport reclaims it after
# an idle spell (see fn_transport_stream.c). Packets stay small: the dynamic
# heap is tight.
TARGET_CFLAGS_palmos-serial  := -Iinclude/palmos -DFN_MAX_PACKET_SIZE=512 -DFN_STREAM_WAKE_IDLE_MS=30000
TARGET_ASFLAGS_palmos-serial :=
TARGET_PLATFORM_SRCS_palmos-serial := src/platform/palmos/fn_channel_serial.c

# Palm OS (Handspring Visor + RP2350 Springboard module over CS1, prc-tools)
# The CS1 ABI header (sbnio.h) lives in fujinet-nio; do not vendor a second copy.
SBNIO_ABI_INCLUDE ?= -I../fujinet-nio/bridges/rp2350-springboard/include
TARGET_CFLAGS_palmos-springboard  := -Iinclude/palmos $(SBNIO_ABI_INCLUDE)
TARGET_ASFLAGS_palmos-springboard :=
# Everything in src/platform/palmos but the cradle serial channel.
TARGET_PLATFORM_SRCS_palmos-springboard := $(filter-out src/platform/palmos/fn_channel_serial.c,$(wildcard src/platform/palmos/*.c))

# Get flags for current target
TARGET_CFLAGS  := $(TARGET_CFLAGS_$(TARGET))
TARGET_ASFLAGS := $(TARGET_ASFLAGS_$(TARGET))
TARGET_PLATFORM_SRCS := $(TARGET_PLATFORM_SRCS_$(TARGET))
