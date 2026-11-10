# SDK selection is shared by static and system-library builds.
PALMOS_SDK_VERSION ?= 3.1
ifeq (,$(filter $(PALMOS_SDK_VERSION),3.1 3.5))
$(error PALMOS_SDK_VERSION must be 3.1 or 3.5)
endif
PALMOS_SDK_FLAGS = -palmos$(PALMOS_SDK_VERSION)
ifeq ($(PALMOS_SDK_VERSION),3.1)
PALMOS_SDK_FLAGS += -DNIO_PALMOS_SDK31
endif
