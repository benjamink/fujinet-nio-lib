# compiler.mk - Compiler Selection
#
# Includes the appropriate compiler configuration based on COMPILER_FAMILY.

ifeq ($(COMPILER_FAMILY),)
    $(error COMPILER_FAMILY not set. Check targets.mk)
endif

# Include the appropriate compiler configuration
ifeq ($(COMPILER_FAMILY),cc65)
    -include makefiles/compiler-cc65.mk
else ifeq ($(COMPILER_FAMILY),cmoc)
    -include makefiles/compiler-cmoc.mk
else ifeq ($(COMPILER_FAMILY),wcc)
    -include makefiles/compiler-wcc.mk
else ifeq ($(COMPILER_FAMILY),gcc)
    -include makefiles/compiler-gcc.mk
else ifeq ($(COMPILER_FAMILY),amigagcc)
    -include makefiles/compiler-amigagcc.mk
else ifeq ($(COMPILER_FAMILY),prcgcc)
    -include makefiles/compiler-prcgcc.mk
else
    $(error Unknown compiler family: $(COMPILER_FAMILY))
endif
