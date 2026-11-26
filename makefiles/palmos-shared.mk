.DEFAULT_GOAL := all
# Installed Palm shared library and application compatibility archive.
CC = m68k-palmos-gcc
OBJCOPY = m68k-palmos-objcopy
BUILD_PRC = build-prc
PILRC = pilrc
PYTHON = python3
B = build/palmos-shared-$(PALMOS_SDK_VERSION)
S = src/platform/palmos/shared
T = examples/palmos-shared
include makefiles/palmos-sdk.mk
PALMFLAGS = -Wall -Werror -O2 $(PALMOS_SDK_FLAGS) -Iinclude/palmos
HANDSPRING_INCLUDE ?=
SBNIO_ABI_INCLUDE ?= -I../fujinet-nio/bridges/rp2350-springboard/include
LIBFLAGS = $(PALMFLAGS) -Iinclude $(SBNIO_ABI_INCLUDE) -mown-gp
CORE = fn_state fn_init fn_shutdown fn_open fn_open_exchange fn_open_translated \
       fn_packet_build_open fn_packet_build_open_ext fn_packet_header \
       fn_packet_checksum fn_packet_checksum_packet fn_checksum_fold \
       fn_packet_parse_common fn_packet_parse_open fn_info fn_packet_build_misc \
       fn_packet_parse_info fn_read fn_packet_build_rw fn_packet_parse_read \
       fn_rw fn_info_close fn_ready fn_clock_common fn_clock_get \
       fn_clock_time_common fn_clock_get_timezone fn_session fn_slip
SERVICECORE = fn_fuji_hsio_call fn_fuji_hsio_get fn_fuji_hsio_set fn_raw fn_file_common fn_file_stat fn_file_read fn_file_write fn_file_mkdir \
       fn_file_list_pages fn_open_headers fn_fuji_info fn_wifi fn_wifi_set fn_wifi_adapter \
       fn_appstore_common fn_appstore_default fn_appstore_stat fn_appstore_read \
       fn_appstore_write fn_appstore_delete fn_appstore_list fn_slot_catalog_common \
       fn_slot_catalog_validate_default fn_slot_catalog_default fn_slot_catalog_get \
       fn_slot_catalog_put fn_slot_catalog_delete fn_slot_catalog_range \
       fn_mount_resolve_common fn_mount_resolve_build fn_mount_resolve_default \
       fn_resolve_mount_target fn_format_mount_display fn_clock_set \
       fn_clock_sync_network_time fn_clock_format_capacity fn_clock_format_common fn_clock_get_format fn_clock_get_tz \
       fn_clock_set_timezone_common fn_clock_set_timezone fn_clock_set_timezone_save \
       fn_disk_common fn_disk_le_get fn_disk_put_u16le fn_disk_put_u32le fn_disk_parse_info \
       fn_disk_mount fn_disk_info fn_disk_read_sector fn_disk_write_sector
DISKCORE = fn_disk_inspect fn_disk_common fn_disk_le_get fn_disk_put_u16le fn_disk_put_u32le fn_disk_parse_info \
       fn_disk_mount fn_disk_info fn_disk_read_sector fn_disk_write_sector
SERVICECORE := $(filter-out $(DISKCORE),$(SERVICECORE))
DISKCOREOBJS = $(addprefix $(B)/core/,$(addsuffix .o,$(DISKCORE)))
SERVICECOREOBJS = $(addprefix $(B)/core/,$(addsuffix .o,$(SERVICECORE)))
COREOBJS = $(addprefix $(B)/core/,$(addsuffix .o,$(CORE)))
SERVICEOBJS = $(patsubst $(S)/services/%.c,$(B)/services/%.o,$(wildcard $(S)/services/*.c))
CLIENTOBJS = $(patsubst $(S)/client/%.c,$(B)/client/%.o,$(wildcard $(S)/client/*.c))
CLIENTCOMMON = fn_clock_format_capacity fn_tcp_open fn_util fn_appstore_list_next_key fn_slot_catalog_next_entry
CLIENTCOMMONOBJS = $(addprefix $(B)/client-common/,$(addsuffix .o,$(CLIENTCOMMON)))
$(B)/client-common/%.o: src/common/%.c $(wildcard include/*.h) $(B)/settings | $(B)
	mkdir -p $(B)/client-common
	$(CC) $(PALMFLAGS) -Iinclude -c $< -o $@
LEGACY = fn_legacy_appkey_state fn_legacy_appkey_buffer fn_legacy_appkey_util \
       fn_legacy_appkey_uri fn_legacy_appkey_mkdir fn_legacy_appkey_set fn_legacy_appkey_read \
       fn_legacy_appkey_read_buffered fn_legacy_appkey_write
LEGACYOBJS = $(addprefix $(B)/legacy/,$(addsuffix .o,$(LEGACY)))
$(B)/services/%.o: $(S)/services/%.c $(S)/service.h $(S)/private.h $(wildcard include/*.h) $(B)/settings | $(B)
	mkdir -p $(B)/services
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/client/%.o: $(S)/client/%.c $(S)/client_private.h $(wildcard include/*.h) $(B)/settings | $(B)
	mkdir -p $(B)/client
	$(CC) $(PALMFLAGS) -Iinclude -c $< -o $@
$(B)/legacy/%.o: src/legacy/%.c $(wildcard include/*.h) $(B)/settings | $(B)
	mkdir -p $(B)/legacy
	$(CC) $(LIBFLAGS) -c $< -o $@
# Test-only padding measures both dispatch and inter-object growth headroom.
# Normal builds have no padding object or storage reservation.
PALMOS_LAYOUT_PAD_BYTES ?= 0
PALMOS_LAYOUT_PAD_POSITION ?= late
ifneq ($(PALMOS_LAYOUT_PAD_BYTES),0)
PADOBJ = $(B)/layout-padding.o
ifeq ($(PALMOS_LAYOUT_PAD_POSITION),early)
EARLYPAD = $(PADOBJ)
else ifeq ($(PALMOS_LAYOUT_PAD_POSITION),late)
LATEPAD = $(PADOBJ)
else
$(error PALMOS_LAYOUT_PAD_POSITION must be early or late)
endif
$(PADOBJ): tests/palmos_layout_padding.S $(B)/settings | $(B)
	$(CC) $(LIBFLAGS) -DNL_LAYOUT_PAD_BYTES=$(PALMOS_LAYOUT_PAD_BYTES) -c $< -o $@
endif
# The Network transport: the NIO device itself, running in the library over NetLib (netlib/).
NETLIB = nio_core nio_net nio_fuji nio_sock_palm
NETLIBOBJS = $(B)/netlib.o $(addprefix $(B)/netlib/,$(addsuffix .o,$(NETLIB)))
# USB needs the Handspring SDK's UsbMgr.h. Without it usb.o is built from usb_none.c: no USB backend, and the
# library does not claim nioLibCapUSB.
ifeq ($(wildcard $(HANDSPRING_INCLUDE)/UsbMgr.h),)
USBSRC = $(S)/usb_none.c
USBDEF = -DNL_NO_USB
else
USBSRC = $(S)/usb.c
USBINC = -I"$(HANDSPRING_INCLUDE)"
endif
BACKEND = fn_palmos_open fn_palmos_close fn_palmos_state
BACKOBJS = $(addprefix $(B)/backend/,$(addsuffix .o,$(BACKEND))) $(B)/backend/fn_palmos_exchange.o $(B)/backend/transport_cs1.o

.PHONY: all verify
all: verify

# Header/flag changes must invalidate objects even when callers override B.
.PHONY: FORCE
$(B)/settings: FORCE | $(B)
	@printf '%s\n' '$(CC) $(LIBFLAGS) $(HANDSPRING_INCLUDE) $(PALMOS_LAYOUT_PAD_BYTES) $(PALMOS_LAYOUT_PAD_POSITION)' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@
$(B)/entry.o $(B)/library.o $(B)/image.o $(B)/owner.o $(B)/services.o $(B)/json.o $(B)/cs1.o $(B)/transport.o $(B)/serial.o $(B)/usb.o $(B)/test.o $(B)/registers.o $(B)/client.o $(COREOBJS) $(SERVICECOREOBJS) $(DISKCOREOBJS) $(BACKOBJS) $(SERVICEOBJS) $(CLIENTOBJS) $(CLIENTCOMMONOBJS) $(LEGACYOBJS) $(NETLIBOBJS): $(B)/settings $(wildcard include/palmos/*.h)

$(B):
	mkdir -p $@
$(B)/library.o: $(S)/library.c include/palmos/NioLib.h $(S)/image.h $(S)/private.h | $(B)
	$(CC) $(LIBFLAGS) $(USBDEF) -c $< -o $@
$(B)/image.o: $(S)/image.c $(S)/image.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/entry.o: $(S)/entry.S | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/owner.o: $(S)/owner.c $(S)/owner.h $(S)/service.h $(wildcard include/*.h) | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/services.o: $(S)/services.c $(S)/owner.h $(S)/service.h include/palmos/NioLib.h $(S)/private.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/json.o: $(S)/json.c $(S)/private.h include/palmos/NioLib.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/cs1.o: $(S)/cs1.c include/palmos/NioLib.h $(S)/private.h | $(B)
	$(CC) $(LIBFLAGS) -DFN_PALMOS_SHARED -c $< -o $@
$(B)/transport.o: $(S)/transport.c $(S)/private.h include/palmos/NioLib.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/usb.o: $(USBSRC) $(S)/private.h include/palmos/NioLib.h | $(B)
	$(CC) $(LIBFLAGS) $(USBINC) -c $< -o $@
$(B)/serial.o: $(S)/serial.c $(S)/private.h include/palmos/NioLib.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/netlib.o: $(S)/netlib.c $(S)/private.h $(wildcard $(S)/netlib/*.h) include/palmos/NioLib.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/netlib/%.o: $(S)/netlib/%.c $(wildcard $(S)/netlib/*.h) | $(B)
	mkdir -p $(B)/netlib
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/backend/transport_cs1.o: src/platform/palmos/fn_transport.c | $(B)
	mkdir -p $(B)/backend
	$(CC) $(LIBFLAGS) -DFN_PALMOS_SHARED -Dfn_transport_init=nl_cs1_init -Dfn_transport_close=nl_cs1_close -Dfn_transport_ready=nl_cs1_ready -Dfn_transport_exchange_buffers=nl_cs1_exchange_buffers -Dfn_transport_exchange=nl_cs1_exchange -c $< -o $@
$(B)/core/%.o: src/common/%.c $(wildcard include/*.h) | $(B)
	mkdir -p $(B)/core
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/backend/fn_palmos_exchange.o: src/platform/palmos/fn_palmos_exchange.c src/platform/palmos/fn_palmos_internal.h $(S)/private.h | $(B)
	mkdir -p $(B)/backend
	$(CC) $(LIBFLAGS) -DFN_PALMOS_SHARED -Dfn_palmos_exchange=nl_original_exchange -c $< -o $@
$(B)/backend/%.o: src/platform/palmos/%.c src/platform/palmos/fn_palmos_internal.h $(S)/private.h | $(B)
	mkdir -p $(B)/backend
	$(CC) $(LIBFLAGS) -DFN_PALMOS_SHARED -c $< -o $@
# Keep the public entries near the dispatch table and place service codecs
# between their callers and the lower-level transport/core implementations.
# The Network transport (netlib.o and its NIO core, netlib/) comes last, so
# the upstream objects keep their distances; services.o reaches it through
# transport.o.
# The linker and image auditor retain their independent range/size checks.
$(B)/NioLib: makefiles/palmos-shared.mk $(PADOBJ) $(B)/entry.o $(B)/library.o $(B)/image.o $(SERVICEOBJS) $(B)/services.o $(LEGACYOBJS) $(SERVICECOREOBJS)  $(B)/owner.o $(B)/transport.o $(DISKCOREOBJS) $(B)/json.o $(COREOBJS) $(B)/cs1.o $(B)/serial.o $(B)/usb.o $(LATEPAD) $(BACKOBJS) $(NETLIBOBJS)
	# prc-tools' documented all-version resource ceiling is 64,720 bytes.
	# Explicit runtime placement keeps memcpy/64-bit helpers reachable from
	# both the public service entries and the lower-level transport code.
	$(CC) $(LIBFLAGS) -nostartfiles -Wl,-T,text_64k,-q,-Map,$@.map -o $@ $(B)/entry.o $(B)/library.o $(B)/image.o $(SERVICEOBJS) $(B)/services.o $(LEGACYOBJS) $(SERVICECOREOBJS) $(EARLYPAD)  $(B)/owner.o $(B)/transport.o -lc -lgcc $(DISKCOREOBJS) $(B)/json.o $(COREOBJS) $(B)/cs1.o $(B)/serial.o $(B)/usb.o $(LATEPAD) $(BACKOBJS) $(NETLIBOBJS)
$(B)/nDat0000.bin: $(B)/NioLib scripts/palmos_shared_image.py
	$(PYTHON) scripts/palmos_shared_image.py image $(B)
$(B)/libr0000.bin: $(B)/NioLib
	$(OBJCOPY) -O binary -j .text $< $@
$(B)/NioLib.prc: $(B)/libr0000.bin $(B)/nDat0000.bin
	$(BUILD_PRC) -L -c NiL1 -n NioLib -o $@ $^

$(B)/test.o: $(T)/test.c include/palmos/NioLib.h | $(B)
	$(CC) $(PALMFLAGS) -Iinclude -c $< -o $@
$(B)/test.o: $(T)/benchmark.h $(T)/write_benchmark.h
$(B)/benchmark.o: $(T)/benchmark.c $(T)/benchmark.h $(wildcard include/*.h include/palmos/*.h) $(B)/settings | $(B)
	$(CC) $(PALMFLAGS) -Iinclude -c $< -o $@
$(B)/write_benchmark.o: $(T)/write_benchmark.c $(T)/write_benchmark.h $(wildcard include/*.h include/palmos/*.h) $(B)/settings | $(B)
	$(CC) $(PALMFLAGS) -Iinclude -c $< -o $@
$(B)/registers.o: $(T)/registers.S | $(B)
	$(CC) $(PALMFLAGS) -c $< -o $@
$(B)/client.o: $(S)/client.c include/palmos/NioLib.h include/palmos/NioLibClient.h | $(B)
	$(CC) $(PALMFLAGS) -Iinclude -c $< -o $@
$(B)/libniolib-client.a: $(B)/client.o $(CLIENTOBJS) $(CLIENTCOMMONOBJS)
	m68k-palmos-ar rcs $@ $^
$(B)/service-link.o: tests/palmos_service_link.c $(wildcard include/palmos/*.h) $(B)/settings | $(B)
	$(CC) $(PALMFLAGS) -Iinclude -c $< -o $@
$(B)/NioLibTest: $(B)/test.o $(B)/benchmark.o $(B)/write_benchmark.o $(B)/registers.o $(B)/service-link.o $(B)/libniolib-client.a
	$(CC) $(PALMFLAGS) -Wl,-Map,$@.map -o $@ $^
$(B)/test-resources.stamp: $(T)/test.rcp | $(B)
	$(PILRC) -q $< $(B)
	touch $@
$(B)/NioLibTest.prc: $(B)/NioLibTest $(B)/test-resources.stamp
	$(BUILD_PRC) -c NiT1 -n NioLibTest -o $@ $(B)/NioLibTest $(B)/tFRM03e8.bin $(B)/tFRM03ea.bin $(B)/tFRM03f2.bin $(B)/tFRM03fc.bin $(B)/tFRM0406.bin $(B)/Talt0403.bin $(B)/Talt040c.bin $(B)/Talt040d.bin $(B)/tAIN03e8.bin

verify: $(B)/NioLib.prc $(B)/NioLibTest.prc
	$(PYTHON) scripts/palmos_shared_image.py audit $(B)
