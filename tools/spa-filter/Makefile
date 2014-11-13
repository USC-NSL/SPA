LEVEL = ../..
TOOLNAME = spa-filter

include $(LEVEL)/Makefile.config

USEDLIBS = spaRuntime.a
LINK_COMPONENTS = jit bitreader bitwriter ipo linker engine

ifeq ($(shell echo "$(LLVM_VERSION_MAJOR).$(LLVM_VERSION_MINOR) >= 3.3" | bc), 1)
LINK_COMPONENTS += irreader
endif
include $(LEVEL)/Makefile.common
