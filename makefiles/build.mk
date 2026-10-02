# build.mk - Core Build Logic
#
# This makefile handles the actual compilation for a specific target.
# It is included by the top-level Makefile.

# Require TARGET to be set
ifeq ($(TARGET),)
    $(error TARGET must be specified)
endif

# Include platform and compiler configuration
-include makefiles/targets.mk
-include makefiles/compiler.mk

# Directory structure
SRCDIR      := src
BUILDDIR    := build
OBJDIR      := obj
DISTDIR     := dist
INCDIR      := include

# Platform-specific source directory
PLATFORM_DIR := $(SRCDIR)/platform/$(PLATFORM)

# Library extension (GCC/amigagcc use .a, CC65 uses .lib)
ifneq (,$(filter $(COMPILER_FAMILY),gcc amigagcc))
    LIBEXT := .a
else
    LIBEXT := .lib
endif

# Output files
LIBRARY     := $(BUILDDIR)/$(PROGRAM)-$(TARGET)$(LIBEXT)

# Common sources (shared across all platforms)
COMMON_SRCS_DEFAULT := $(SRCDIR)/common/fn_slip.c \
                       $(SRCDIR)/common/fn_session.c \
                       $(SRCDIR)/common/fn_packet.c \
                       $(SRCDIR)/common/fn_checksum_fold.c \
                       $(SRCDIR)/common/fn_packet_checksum.c \
                       $(SRCDIR)/common/fn_packet_checksum_packet.c \
                       $(SRCDIR)/common/fn_packet_header.c \
                       $(SRCDIR)/common/fn_packet_build_open.c \
                       $(SRCDIR)/common/fn_packet_build_rw.c \
                       $(SRCDIR)/common/fn_packet_build_misc.c \
                       $(SRCDIR)/common/fn_packet_parse_common.c \
                       $(SRCDIR)/common/fn_packet_parse_open.c \
                       $(SRCDIR)/common/fn_packet_parse_read.c \
                       $(SRCDIR)/common/fn_packet_parse_info.c \
                       $(SRCDIR)/common/fn_state.c \
                       $(SRCDIR)/common/fn_init.c \
                       $(SRCDIR)/common/fn_shutdown.c \
                       $(SRCDIR)/common/fn_open.c \
                       $(SRCDIR)/common/fn_rw.c \
                       $(SRCDIR)/common/fn_info_close.c \
                       $(SRCDIR)/common/fn_util.c \
                       $(SRCDIR)/common/fn_ext.c \
                       $(SRCDIR)/common/fn_clock_common.c \
                       $(SRCDIR)/common/fn_clock_time_common.c \
                       $(SRCDIR)/common/fn_clock_get.c \
                       $(SRCDIR)/common/fn_clock_set.c \
                       $(SRCDIR)/common/fn_clock_sync_network_time.c \
                       $(SRCDIR)/common/fn_clock_format_common.c \
                       $(SRCDIR)/common/fn_clock_get_format.c \
                       $(SRCDIR)/common/fn_clock_get_tz.c \
                       $(SRCDIR)/common/fn_clock_get_timezone.c \
                       $(SRCDIR)/common/fn_clock_set_timezone_common.c \
                       $(SRCDIR)/common/fn_clock_set_timezone.c \
                       $(SRCDIR)/common/fn_clock_set_timezone_save.c \
                       $(SRCDIR)/common/fn_appstore_common.c \
                       $(SRCDIR)/common/fn_appstore_default.c \
                       $(SRCDIR)/common/fn_appstore_stat.c \
                       $(SRCDIR)/common/fn_appstore_read.c \
                       $(SRCDIR)/common/fn_appstore_write.c \
                       $(SRCDIR)/common/fn_appstore_delete.c \
                       $(SRCDIR)/common/fn_appstore_list.c \
                       $(SRCDIR)/common/fn_appstore_list_next_key.c \
                       $(SRCDIR)/common/fn_slot_catalog_common.c \
                       $(SRCDIR)/common/fn_slot_catalog_validate_default.c \
                       $(SRCDIR)/common/fn_slot_catalog_default.c \
                       $(SRCDIR)/common/fn_slot_catalog_get.c \
                       $(SRCDIR)/common/fn_slot_catalog_put.c \
                       $(SRCDIR)/common/fn_slot_catalog_delete.c \
                       $(SRCDIR)/common/fn_slot_catalog_range.c \
                       $(SRCDIR)/common/fn_slot_catalog_next_entry.c \
                       $(SRCDIR)/common/fn_mount_resolve_common.c \
                       $(SRCDIR)/common/fn_mount_resolve_build.c \
                       $(SRCDIR)/common/fn_mount_resolve_default.c \
                       $(SRCDIR)/common/fn_resolve_mount_target.c \
                       $(SRCDIR)/common/fn_format_mount_display.c \
                       $(SRCDIR)/common/fn_disk.c \
                       $(SRCDIR)/common/fn_raw.c
COMMON_SRCS_DEFAULT += $(SRCDIR)/common/fn_wifi.c $(SRCDIR)/common/fn_wifi_adapter.c

LEGACY_SRCS_DEFAULT := $(SRCDIR)/legacy/fn_legacy_appkey_state.c \
                       $(SRCDIR)/legacy/fn_legacy_appkey_util.c \
                       $(SRCDIR)/legacy/fn_legacy_appkey_set.c \
                       $(SRCDIR)/legacy/fn_legacy_appkey_read.c \
                       $(SRCDIR)/legacy/fn_legacy_appkey_write.c

COMMON_SRCS_BBC := $(SRCDIR)/common/fn_util.c \
                   $(SRCDIR)/common/fn_ext.c \
                   $(SRCDIR)/common/fn_clock_time_common.c \
                   $(SRCDIR)/common/fn_clock_get.c \
                   $(SRCDIR)/common/fn_clock_set.c \
                   $(SRCDIR)/common/fn_clock_sync_network_time.c \
                   $(SRCDIR)/common/fn_clock_format_common.c \
                   $(SRCDIR)/common/fn_clock_get_format.c \
                   $(SRCDIR)/common/fn_clock_get_tz.c \
                   $(SRCDIR)/common/fn_clock_get_timezone.c \
                   $(SRCDIR)/common/fn_clock_set_timezone_common.c \
                   $(SRCDIR)/common/fn_clock_set_timezone.c \
                   $(SRCDIR)/common/fn_clock_set_timezone_save.c \
                   $(SRCDIR)/common/fn_appstore_stat.c \
                   $(SRCDIR)/common/fn_appstore_delete.c \
                   $(SRCDIR)/common/fn_appstore_list.c \
                   $(SRCDIR)/common/fn_appstore_list_next_key.c \
                   $(SRCDIR)/common/fn_slot_catalog_common.c \
                   $(SRCDIR)/common/fn_slot_catalog_get.c \
                   $(SRCDIR)/common/fn_slot_catalog_put.c \
                   $(SRCDIR)/common/fn_slot_catalog_delete.c \
                   $(SRCDIR)/common/fn_slot_catalog_range.c \
                   $(SRCDIR)/common/fn_slot_catalog_next_entry.c \
                   $(SRCDIR)/common/fn_mount_resolve_common.c \
                   $(SRCDIR)/common/fn_mount_resolve_build.c \
                   $(SRCDIR)/common/fn_resolve_mount_target.c \
                    $(SRCDIR)/common/fn_format_mount_display.c
COMMON_SRCS_BBC += $(SRCDIR)/platform/bbc/fn_wifi.c $(SRCDIR)/platform/bbc/fn_wifi_adapter.c

# BBC fn_appstore_read/write are supplied by platform/bbc assembly sources.
# Other platforms retain the portable implementations in COMMON_SRCS_DEFAULT.

ifeq ($(PLATFORM),bbc)
COMMON_SRCS := $(COMMON_SRCS_BBC)
LEGACY_SRCS :=
else
COMMON_SRCS := $(COMMON_SRCS_DEFAULT)
LEGACY_SRCS := $(LEGACY_SRCS_DEFAULT)
endif

ifeq ($(TRANSPORT_FAMILY),stream)
COMMON_SRCS += $(SRCDIR)/common/fn_transport_stream.c
endif

# Platform-specific sources
ifneq ($(TARGET_PLATFORM_SRCS),)
PLATFORM_SRCS := $(TARGET_PLATFORM_SRCS)
else
PLATFORM_SRCS := $(wildcard $(PLATFORM_DIR)/*.c)
endif
ifeq ($(PLATFORM),bbc)
# Replaced by smaller platform/bbc assembly implementations.
PLATFORM_SRCS := $(filter-out $(PLATFORM_DIR)/fn_file_call.c \
                              $(PLATFORM_DIR)/fn_open.c \
                              $(PLATFORM_DIR)/fn_open_long.c \
                              $(PLATFORM_DIR)/fn_tcp_open.c,$(PLATFORM_SRCS))
endif
PLATFORM_ASMS := $(wildcard $(PLATFORM_DIR)/*.s)
PLATFORM_ASM_INCLUDES := $(wildcard $(PLATFORM_DIR)/*.inc)

# All sources
SOURCES := $(COMMON_SRCS) $(LEGACY_SRCS) $(PLATFORM_SRCS)

# Object files
OBJECTS_C   := $(SOURCES:.c=.o)
OBJECTS_ASM := $(PLATFORM_ASMS:.s=.o)
OBJECTS     := $(patsubst $(SRCDIR)/%,$(OBJDIR)/$(TARGET)/%,$(OBJECTS_C)) \
               $(patsubst $(SRCDIR)/%,$(OBJDIR)/$(TARGET)/%,$(OBJECTS_ASM))

# Dependency files
DEPENDS := $(OBJECTS:.o=.d)

# Include paths
INCLUDES := -I$(INCDIR)

# Compiler flags
CFLAGS   += $(INCLUDES) $(TARGET_CFLAGS)
ASFLAGS  += $(INCLUDES) $(TARGET_ASFLAGS)
ifeq ($(PLATFORM),bbc)
include makefiles/cc65-env.mk
ASFLAGS  += --asm-include-dir $(CC65_HOME)/libsrc/bbc
ASFLAGS  += --asm-include-dir $(CC65_HOME)/asminc
endif

# Phony targets
.PHONY: lib clean-obj

# Main build target (named 'lib' to avoid conflict with build/ directory)
lib: $(LIBRARY)

# Include dependency files
-include $(DEPENDS)

# Create directories
$(OBJDIR)/$(TARGET):
	@mkdir -p $@

$(OBJDIR)/$(TARGET)/common:
	@mkdir -p $@

$(OBJDIR)/$(TARGET)/legacy:
	@mkdir -p $@

$(OBJDIR)/$(TARGET)/platform/$(PLATFORM):
	@mkdir -p $@

$(BUILDDIR):
	@mkdir -p $@

# Assemble ASM files (CC65 only)
$(OBJDIR)/$(TARGET)/platform/$(PLATFORM)/%.o: $(SRCDIR)/platform/$(PLATFORM)/%.s $(PLATFORM_ASM_INCLUDES) | $(OBJDIR)/$(TARGET)/platform/$(PLATFORM)
	@echo "  AS $<"
	$(CC) -t $(TARGET) -c $(ASFLAGS) --listing $(@:.o=.lst) -o $@ $<

# Compile C files
ifneq (,$(filter $(COMPILER_FAMILY),gcc amigagcc))
# GCC / amiga-gcc compilation
$(OBJDIR)/$(TARGET)/common/%.o: $(SRCDIR)/common/%.c | $(OBJDIR)/$(TARGET)/common
	@echo "  CC $<"
	$(CC) -c $(CFLAGS) -MMD -MF $(@:.o=.d) -o $@ $<

$(OBJDIR)/$(TARGET)/legacy/%.o: $(SRCDIR)/legacy/%.c | $(OBJDIR)/$(TARGET)/legacy
	@echo "  CC $<"
	$(CC) -c $(CFLAGS) -MMD -MF $(@:.o=.d) -o $@ $<

$(OBJDIR)/$(TARGET)/platform/$(PLATFORM)/%.o: $(SRCDIR)/platform/$(PLATFORM)/%.c | $(OBJDIR)/$(TARGET)/platform/$(PLATFORM)
	@echo "  CC $<"
	$(CC) -c $(CFLAGS) -MMD -MF $(@:.o=.d) -o $@ $<
else ifeq ($(COMPILER_FAMILY),wcc)
# Open Watcom compilation
$(OBJDIR)/$(TARGET)/common/%.o: $(SRCDIR)/common/%.c | $(OBJDIR)/$(TARGET)/common
	@echo "  CC $<"
	$(CC) $(CFLAGS) -fo=$@ -fr=$(@:.o=.err) $<

$(OBJDIR)/$(TARGET)/legacy/%.o: $(SRCDIR)/legacy/%.c | $(OBJDIR)/$(TARGET)/legacy
	@echo "  CC $<"
	$(CC) $(CFLAGS) -fo=$@ -fr=$(@:.o=.err) $<

$(OBJDIR)/$(TARGET)/platform/$(PLATFORM)/%.o: $(SRCDIR)/platform/$(PLATFORM)/%.c | $(OBJDIR)/$(TARGET)/platform/$(PLATFORM)
	@echo "  CC $<"
	$(CC) $(CFLAGS) -fo=$@ -fr=$(@:.o=.err) $<
else
# CC65 compilation
$(OBJDIR)/$(TARGET)/common/%.o: $(SRCDIR)/common/%.c | $(OBJDIR)/$(TARGET)/common
	@echo "  CC $<"
	$(CC) -t $(TARGET) -c $(CFLAGS) --create-dep $(@:.o=.d) --listing $(@:.o=.lst) -o $@ $<

$(OBJDIR)/$(TARGET)/legacy/%.o: $(SRCDIR)/legacy/%.c | $(OBJDIR)/$(TARGET)/legacy
	@echo "  CC $<"
	$(CC) -t $(TARGET) -c $(CFLAGS) --create-dep $(@:.o=.d) --listing $(@:.o=.lst) -o $@ $<

$(OBJDIR)/$(TARGET)/platform/$(PLATFORM)/%.o: $(SRCDIR)/platform/$(PLATFORM)/%.c | $(OBJDIR)/$(TARGET)/platform/$(PLATFORM)
	@echo "  CC $<"
	$(CC) -t $(TARGET) -c $(CFLAGS) --create-dep $(@:.o=.d) --listing $(@:.o=.lst) -o $@ $<
endif

# Create library
ifneq (,$(filter $(COMPILER_FAMILY),gcc amigagcc))
$(LIBRARY): $(OBJECTS) | $(BUILDDIR)
	@echo "  AR $@"
	$(AR) rcs $@ $(OBJECTS)
	@echo "  Created $@"
else ifeq ($(COMPILER_FAMILY),wcc)
$(LIBRARY): $(OBJECTS) | $(BUILDDIR)
	@echo "  AR $@"
	rm -f $@
	$(call AR_CMD,$@,$(OBJECTS))
	@echo "  Created $@"
else
$(LIBRARY): $(OBJECTS) | $(BUILDDIR)
	@echo "  AR $@"
	rm -f $@
	$(AR) a $@ $(OBJECTS)
	@echo "  Created $@"
endif

# Clean object files for this target
clean-obj:
	rm -rf $(OBJDIR)/$(TARGET)
