# fujinet-nio-lib Makefile
#
# A clean, multi-platform 6502 library for FujiNet-NIO
#
# Usage:
#   make            - Build all targets
#   make atari      - Build for Atari
#   make bbc        - Build for BBC B
#   make bbc-clib   - Build for BBC B with clib ROM
#   make c64        - Build for Commodore 64
#   make linux      - build for linux/posix environment
#   make clean      - Remove build artifacts
#   make help       - Show this help

# Supported targets
# TARGETS = atari apple2 apple2enh bbc c64 coco msdos linux
TARGETS = atari bbc bbc-clib c64 msdos-serial msdos-ioctl msdos-f5 linux amiga
EXTRA_TARGETS = amiga-driver palmos-serial palmos-springboard

# Output library name
override PROGRAM := fujinet-nio

# Phony targets
.PHONY: all clean help disk-bbc disk-bbc-clib test-legacy test-mount-resolve \
	test-appstore-read test-list-pages test-slot-catalog test-bbc test-bbc-scripted \
	test-bbc-real test-fuji test-wifi test-disk test-disk-context test-session test-stream-wake test-open-ext test-open-wire test-ready-wire test-write-partial test-library-link \
	test-amiga-transport test-c64-transport test-palmos-transport test-atari-transport test-atari-hsio test-atari-proc test-atari-read-profile test-atari-list-pages test-atari-appkey-uri test-atari-parse-read test-atari-client-buffers test-atari-network-read test-atari-network-write test-atari-network-open test-atari-compact test-raw-transport-retry test \
	test-all-build check msdos $(TARGETS) $(EXTRA_TARGETS)

# Default target: build all
all:
	@echo "========================================="
	@echo "FujiNet-NIO Library Build"
	@echo "========================================="
	@for target in $(TARGETS); do \
		echo ""; \
		echo "Building for $$target..."; \
		echo "-----------------------------------------"; \
		$(MAKE) -f makefiles/build.mk TARGET=$$target PROGRAM=$(PROGRAM) lib || exit 1; \
	done
	@echo ""
	@echo "========================================="
	@echo "Build complete!"
	@echo "========================================="

# Individual platform targets
$(TARGETS) $(EXTRA_TARGETS):
	@echo "Building for $@..."
	$(MAKE) -f makefiles/build.mk TARGET=$@ PROGRAM=$(PROGRAM) lib

msdos: msdos-serial msdos-ioctl msdos-f5

# Clean all targets
clean:
	@echo "Cleaning build artifacts..."
	rm -rf build/ obj/ dist/
	$(MAKE) -C examples clean-all
	rm -rf examples/ssd
	@echo "Done."

disk-bbc:
	$(MAKE) bbc
	$(MAKE) -C examples TARGET=bbc disk

disk-bbc-clib:
	$(MAKE) bbc-clib
	$(MAKE) -C examples TARGET=bbc-clib disk

test-bbc:
	@echo "========================================="
	@echo "BBC test scaffold"
	@echo "========================================="
	$(MAKE) bbc
	$(MAKE) -C examples TARGET=bbc all
	@echo ""
	@echo "Built BBC library, supported BBC examples, and BBC disk images."
	@echo "See integration-tests/beebium/README.md for scripted and real E2E setup."

test-bbc-scripted:
	@echo "Running scripted BBC test scaffold..."
	$(MAKE) bbc
	$(MAKE) -C examples TARGET=bbc all
	@if [ -d integration-tests/beebium ]; then \
		cd integration-tests/beebium && ./run_pytest.sh -q scripted; \
	else \
		echo "integration-tests/beebium not present"; \
		exit 1; \
	fi

test-bbc-real:
	@echo "Running real BBC test scaffold..."
	$(MAKE) bbc
	$(MAKE) -C examples TARGET=bbc all
	@if [ -d integration-tests/beebium ]; then \
		cd integration-tests/beebium && ./run_pytest.sh -q real; \
	else \
		echo "integration-tests/beebium not present"; \
		exit 1; \
	fi

test-legacy:
	@echo "Running legacy compatibility wire tests..."
	./tests/run_legacy_appkey_wire_test.sh

test-mount-resolve:
	@echo "Running mount resolve wire tests..."
	bash ./tests/run_mount_resolve_wire_test.sh

test-appstore-read:
	@echo "Running app-store read wire tests..."
	bash ./tests/run_appstore_read_wire_test.sh

test-list-pages:
	@echo "Running paged directory listing wire tests..."
	bash ./tests/run_list_pages_wire_test.sh

test-atari-list-pages:
	@echo "Running 6502 ListPages independent wire and boundary vectors..."
	bash ./tests/run_atari_list_pages_test.sh

test-atari-appkey-uri:
	@echo "Running 6502 AppKey URI independent vectors..."
	bash ./tests/run_atari_appkey_uri_test.sh

test-write-partial:
	@echo "Running sequential partial write wire vectors..."
	bash ./tests/run_write_partial_wire_test.sh

test-atari-network-read:
	@echo "Running 6502 network read API vectors..."
	bash ./tests/run_atari_network_read_test.sh

test-atari-network-write:
	@echo "Running 6502 network write API vectors..."
	bash ./tests/run_atari_network_write_test.sh

test-atari-network-open:
	@echo "Running 6502 network open exchange API vectors..."
	bash ./tests/run_atari_network_open_test.sh

test-atari-client-buffers:
	@echo "Running 6502 client-owned buffer and archive-link tests..."
	bash ./tests/run_atari_client_buffers_test.sh

test-atari-parse-read:
	@echo "Running 6502 network Read parser independent vectors..."
	bash ./tests/run_atari_parse_read_test.sh

test-slot-catalog:
	@echo "Running slot catalogue wire tests..."
	bash ./tests/run_slot_catalog_wire_test.sh

test-fuji:
	@echo "Running FujiDevice wire tests..."
	bash ./tests/run_fuji_wire_test.sh

test-wifi:
	@echo "Running Wi-Fi API wire tests..."
	bash ./tests/run_wifi_wire_test.sh

test-disk:
	@echo "Running DiskDevice API wire tests..."
	bash ./tests/run_disk_wire_test.sh

test-disk-context:
	@echo "Running independent DiskDevice context tests..."
	bash ./tests/run_disk_context_test.sh

test-session:
	@echo "Running channel/session wire tests..."
	bash ./tests/run_session_wire_test.sh

test-stream-wake:
	@echo "Running stream transport idle-reclaim tests..."
	bash ./tests/run_stream_wake_test.sh

test-open-ext:
	@echo "Running Open-extension wire test..."
	bash ./tests/run_open_ext_wire_test.sh

test-open-wire:
	@echo "Running fn_open / fn_open_translated wire test..."
	bash ./tests/run_open_wire_test.sh

test-ready-wire:
	@echo "Running fn_ready / fn_read_available wire test..."
	bash ./tests/run_ready_wire_test.sh

test-large-read:
	bash ./tests/run_large_read_wire_test.sh

test-library-link: linux
	@echo "Running public API library-link test..."
	bash ./tests/run_library_link_test.sh

test-amiga-transport:
	@echo "Running Amiga broker-transport host tests..."
	bash ./tests/run_amiga_transport_host_test.sh

test-palmos-transport:
	@echo "Running Palm OS Springboard transport host tests..."
	bash ./tests/run_palmos_transport_host_test.sh

test-atari-transport:
	@echo "Running Atari assembled SIOV/DCB transport test..."
	bash ./tests/run_atari_transport_sim_test.sh

test-atari-read-profile:
	@echo "Running Atari fn_disk_read_sector cycle profile..."
	bash ./tests/run_atari_read_profile_test.sh

test-atari-hsio:
	@echo "Running Atari high-speed SIO enable/disable test..."
	bash ./tests/run_atari_hsio_test.sh

test-atari-proc:
	@echo "Running Atari PROC interrupt handler test..."
	bash ./tests/run_atari_proc_test.sh

test-atari-compact:
	bash ./tests/run_atari_compact_test.sh

test-c64-transport:
	@echo "Running C64 IEC transport host test..."
	bash ./tests/run_c64_transport_host_test.sh

test-raw-transport-retry:
	@echo "Running raw-call transport replay guard..."
	bash ./tests/run_raw_transport_retry_test.sh

test-all-build:
	@echo "Building all configured library targets..."
	$(MAKE) all

test-u1mb-clear:
	bash tests/run_u1mb_clear_test.sh

test-fuji-hsio:
	bash tests/run_fuji_hsio_test.sh

test-package-atari:
	python3 tests/test_package_atari.py

test-release:
	python3 tests/test_release.py

test: test-fuji-hsio test-package-atari test-release

test: test-u1mb-clear test-library-link test-legacy test-mount-resolve test-appstore-read test-list-pages test-atari-list-pages test-atari-appkey-uri test-atari-parse-read test-atari-client-buffers test-atari-network-read test-atari-network-write test-atari-network-open test-slot-catalog test-fuji test-wifi test-disk test-disk-context test-session test-stream-wake test-open-ext test-open-wire test-ready-wire test-large-read test-write-partial test-amiga-transport test-c64-transport test-palmos-transport test-atari-transport test-atari-hsio test-atari-proc test-atari-read-profile test-atari-compact test-raw-transport-retry

# Fast required change check: compile every configured target, then run the
# host-side tests and public archive-link test. Keep this as a recipe rather
# than parallel prerequisites so a clean build always has a deterministic
# order.
check:
	@echo "Running fast all-target library check..."
	$(MAKE) all
	@for target in $(EXTRA_TARGETS); do \
		echo ""; \
		echo "Building extra target $$target..."; \
		$(MAKE) $$target || exit 1; \
	done
	$(MAKE) test

# Help
help:
	@echo "FujiNet-NIO Library Build System"
	@echo ""
	@echo "Usage:"
	@echo "  make            - Build all targets"
	@echo "  make <target>   - Build specific target (atari, apple2, coco, etc.)"
	@echo "  make disk-bbc   - Build BBC example disk image(s)"
	@echo "  make disk-bbc-clib - Build BBC CLIB example disk image(s)"
	@echo "  make test-legacy - Run legacy compatibility wire tests"
	@echo "  make test-appstore-read - Run app-store read wire tests"
	@echo "  make test-slot-catalog - Run slot catalogue wire tests"
	@echo "  make test-wifi - Run Wi-Fi API wire and buffer tests"
	@echo "  make test-disk - Run DiskDevice API wire tests"
	@echo "  make test-session - Run channel/session wire tests"
	@echo "  make test-library-link - Build Linux library and link a public API consumer"
	@echo "  make test-amiga-transport - Run Amiga broker-client host tests"
	@echo "  make test-stream-wake - Run stream transport idle-reclaim tests"
	@echo "  make test-palmos-transport - Run Palm OS Springboard client host tests"
	@echo "  make test-all-build - Build all configured library targets"
	@echo "  make check - Build all targets and run the fast host-side test suite"
	@echo "  make test - Run all host-side wire tests"
	@echo "  make clean      - Remove all build artifacts"
	@echo "  make help       - Show this help message"
	@echo ""
	@echo "Supported targets:"
	@echo "  atari       - Atari 8-bit (cc65)"
	@echo "  bbc         - BBC Micro (cc65)"
	@echo "  bbc-clib    - BBC Micro CLIB ROM target (cc65)"
	@echo "  msdos       - MS-DOS serial, IOCTL, and F5 libraries"
	@echo "  msdos-serial - MS-DOS direct COM backend (Open Watcom)"
	@echo "  msdos-ioctl  - MS-DOS FUJINET.SYS IOCTL backend (Open Watcom)"
	@echo "  msdos-f5     - MS-DOS FUJINET.SYS INT F5 backend (Open Watcom)"
	@echo "  linux       - Linux/Posix (gcc)"
	@echo "  palmos-serial - Palm OS 3.1+ over the cradle serial port (prc-tools)"
	@echo "  palmos-springboard - Palm OS over a Springboard module's CS1 link (prc-tools)"
	@echo ""
	@echo "Environment:"
	@echo "  CC65_HOME   - cc65 checkout root (required for bbc/bbc-clib)"
	@echo "  CC65_BASE   - accepted as an alias for CC65_HOME"
	@echo "  SBNIO_ABI_INCLUDE - -I path to sbnio.h for palmos-springboard"
	@echo ""
	@echo "Output:"
	@echo "  Libraries are placed in build/"
	@echo "  Object files are placed in obj/"

.PHONY: test-atari-stream
test-atari-stream:
	soft65c02_unit -b build/tests/atari-stream -i tests/atari_stream_soft65.yaml
	soft65c02_unit -b build/tests/atari-stream-driver -i tests/atari_stream_driver_soft65.yaml
test: test-atari-stream

.PHONY: test-bbc-api-link
test-bbc-api-link: bbc bbc-clib
	python3 tests/run_bbc_api_link_test.py

test: test-bbc-api-link

.PHONY: test-bbc-ready
test-bbc-ready:
	bash tests/run_bbc_ready_test.sh
test: test-bbc-ready

.PHONY: test-bbc-headers
test-bbc-headers:
	bash tests/run_bbc_headers_test.sh
test: test-bbc-headers
