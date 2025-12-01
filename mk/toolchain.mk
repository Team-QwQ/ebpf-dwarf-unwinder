# Auto-selected toolchains based on ARCH. Override TOOLCHAIN_<arch>
# or CROSS_COMPILE from the command line when custom compilers are needed.

ARCH ?= x86_64

SUPPORTED_ARCHES := x86_64 arm64 mips32
ifeq ($(filter $(ARCH),$(SUPPORTED_ARCHES)),)
$(error Unsupported ARCH "$(ARCH)". Choose one of: $(SUPPORTED_ARCHES))
endif

TOOLCHAIN_x86_64 ?=
TOOLCHAIN_arm64 ?= aarch64-linux-gnu-
TOOLCHAIN_mips32 ?= mipsel-linux-gnu-

CROSS_COMPILE ?= $(TOOLCHAIN_$(ARCH))
CC ?= $(CROSS_COMPILE)gcc
AR ?= $(CROSS_COMPILE)ar
RANLIB ?= $(CROSS_COMPILE)ranlib

DWUNW_ARCH_CFLAGS_x86_64 ?= -DDWUNW_TARGET_X86_64
DWUNW_ARCH_CFLAGS_arm64 ?= -DDWUNW_TARGET_ARM64
DWUNW_ARCH_CFLAGS_mips32 ?= -DDWUNW_TARGET_MIPS32
DWUNW_ARCH_CFLAGS ?= $(DWUNW_ARCH_CFLAGS_$(ARCH))
