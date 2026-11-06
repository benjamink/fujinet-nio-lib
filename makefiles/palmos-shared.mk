# Independent milestone target: no common sources or existing target flags.
CC = m68k-palmos-gcc
OBJCOPY = m68k-palmos-objcopy
BUILD_PRC = build-prc
PILRC = pilrc
PYTHON = python3
B = build/palmos-shared-usb
S = src/platform/palmos/shared
T = examples/palmos-shared
PALMFLAGS = -Wall -Werror -O2 -palmos3.5 -Iinclude/palmos
HANDSPRING_INCLUDE ?= -I/handspring/Include
SBNIO_ABI_INCLUDE ?= -I../fujinet-nio/bridges/rp2350-springboard/include
LIBFLAGS = $(PALMFLAGS) -Iinclude $(SBNIO_ABI_INCLUDE) -mown-gp
CORE = fn_state fn_init fn_shutdown fn_open fn_open_exchange fn_open_translated \
       fn_packet_build_open fn_packet_build_open_ext fn_packet_header \
       fn_packet_checksum fn_packet_checksum_packet fn_checksum_fold \
       fn_packet_parse_common fn_packet_parse_open fn_info fn_packet_build_misc \
       fn_packet_parse_info fn_read fn_packet_build_rw fn_packet_parse_read \
       fn_rw fn_info_close fn_ready fn_clock_common fn_clock_get \
       fn_clock_time_common fn_clock_get_timezone fn_session fn_slip
COREOBJS = $(addprefix $(B)/core/,$(addsuffix .o,$(CORE)))
BACKEND = fn_palmos_open fn_palmos_close fn_palmos_state
BACKOBJS = $(addprefix $(B)/backend/,$(addsuffix .o,$(BACKEND))) $(B)/backend/fn_palmos_exchange.o $(B)/backend/transport_cs1.o

.PHONY: all verify
all: verify

$(B):
	mkdir -p $@
$(B)/library.o: $(S)/library.c include/palmos/NioLib.h $(S)/image.h $(S)/private.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/image.o: $(S)/image.c $(S)/image.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/entry.o: $(S)/entry.S | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/services.o: $(S)/services.c include/palmos/NioLib.h $(S)/private.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/json.o: $(S)/json.c $(S)/private.h include/palmos/NioLib.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/cs1.o: $(S)/cs1.c include/palmos/NioLib.h $(S)/private.h | $(B)
	$(CC) $(LIBFLAGS) -DFN_PALMOS_SHARED -c $< -o $@
$(B)/transport.o: $(S)/transport.c $(S)/private.h include/palmos/NioLib.h | $(B)
	$(CC) $(LIBFLAGS) -c $< -o $@
$(B)/usb.o: $(S)/usb.c $(S)/private.h include/palmos/NioLib.h | $(B)
	$(CC) $(LIBFLAGS) $(HANDSPRING_INCLUDE) -c $< -o $@
$(B)/serial.o: $(S)/serial.c $(S)/private.h include/palmos/NioLib.h | $(B)
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
$(B)/NioLib: $(B)/entry.o $(B)/library.o $(B)/image.o $(B)/services.o $(B)/json.o $(B)/cs1.o $(B)/transport.o $(B)/serial.o $(B)/usb.o $(COREOBJS) $(BACKOBJS)
	$(CC) $(LIBFLAGS) -nostartfiles -Wl,-q,-Map,$@.map -o $@ $^
$(B)/nDat0000.bin: $(B)/NioLib scripts/palmos_shared_image.py
	$(PYTHON) scripts/palmos_shared_image.py image $(B)
$(B)/libr0000.bin: $(B)/NioLib
	$(OBJCOPY) -O binary -j .text $< $@
$(B)/NioLib.prc: $(B)/libr0000.bin $(B)/nDat0000.bin
	$(BUILD_PRC) -L -c NiL1 -n NioLib -o $@ $^

$(B)/test.o: $(T)/test.c include/palmos/NioLib.h | $(B)
	$(CC) $(PALMFLAGS) -Iinclude -c $< -o $@
$(B)/registers.o: $(T)/registers.S | $(B)
	$(CC) $(PALMFLAGS) -c $< -o $@
$(B)/client.o: $(S)/client.c include/palmos/NioLib.h include/palmos/NioLibClient.h | $(B)
	$(CC) $(PALMFLAGS) -Iinclude -c $< -o $@
$(B)/libniolib-client.a: $(B)/client.o
	m68k-palmos-ar rcs $@ $^
$(B)/NioLibTest: $(B)/test.o $(B)/registers.o $(B)/libniolib-client.a
	$(CC) $(PALMFLAGS) -Wl,-Map,$@.map -o $@ $^
$(B)/test-resources.stamp: $(T)/test.rcp | $(B)
	$(PILRC) -q $< $(B)
	touch $@
$(B)/NioLibTest.prc: $(B)/NioLibTest $(B)/test-resources.stamp
	$(BUILD_PRC) -c NiT1 -n NioLibTest -o $@ $(B)/NioLibTest $(B)/tFRM03e8.bin $(B)/tFRM03ea.bin $(B)/tFRM03f2.bin $(B)/tAIN03e8.bin

verify: $(B)/NioLib.prc $(B)/NioLibTest.prc
	$(PYTHON) scripts/palmos_shared_image.py audit $(B)
