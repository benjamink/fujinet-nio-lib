# fujinet-nio-lib Makefile
#
# A clean, multi-platform 6502 library for FujiNet-NIO
#
# Usage:
#   make            - Build all targets
#   make atari      - Build for Atari
#   make bbc        - Build for BBC B
#   make bbc-clib   - Build for BBC B with clib ROM
#   make linux      - build for linux/posix environment
#   make clean      - Remove build artifacts
#   make help       - Show this help

# Supported targets
# TARGETS = atari apple2 apple2enh bbc c64 coco msdos linux
TARGETS = atari bbc bbc-clib msdos-serial msdos-ioctl msdos-f5 linux amiga
EXTRA_TARGETS = amiga-driver

# Output library name
override PROGRAM := fujinet-nio

# Phony targets
.PHONY: all clean help disk-bbc disk-bbc-clib test-legacy test-mount-resolve \
	test-appstore-read test-slot-catalog test-bbc test-bbc-scripted \
	test-bbc-real test-fuji test-wifi test-disk test-disk-context test-session test-open-ext test-library-link \
	test-amiga-transport test \
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

test-open-ext:
	@echo "Running Open-extension wire test..."
	sh ./tests/run_open_ext_wire_test.sh

test-library-link: linux
	@echo "Running public API library-link test..."
	bash ./tests/run_library_link_test.sh

test-amiga-transport:
	@echo "Running Amiga broker-transport host tests..."
	bash ./tests/run_amiga_transport_host_test.sh

test-all-build:
	@echo "Building all configured library targets..."
	$(MAKE) all

test: test-library-link test-legacy test-mount-resolve test-appstore-read test-slot-catalog test-fuji test-wifi test-disk test-disk-context test-session test-open-ext test-amiga-transport

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
	@echo ""
	@echo "Environment:"
	@echo "  CC65_HOME   - cc65 checkout root (required for bbc/bbc-clib)"
	@echo "  CC65_BASE   - accepted as an alias for CC65_HOME"
	@echo ""
	@echo "Output:"
	@echo "  Libraries are placed in build/"
	@echo "  Object files are placed in obj/"
