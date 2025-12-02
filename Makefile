# SPDX-License-Identifier: MIT

include mk/toolchain.mk

SRC_ROOT := src
INCLUDE_ROOT := include
BUILD_ROOT ?= build/$(ARCH)
OBJ_ROOT := $(BUILD_ROOT)/obj
LIB_TARGET := $(BUILD_ROOT)/libdwunw.a
TEST_FIXTURE := $(BUILD_ROOT)/fixtures/dwarf_fixture
EXAMPLE_MEMLEAK_SRC := examples/bpf_memleak/memleak_user.c
EXAMPLE_MEMLEAK_TARGET := $(BUILD_ROOT)/examples/bpf_memleak/memleak_user
MEMLEAK_BCC_DIR := examples/memleak_bcc_dwunw
MEMLEAK_BCC_USER_SRC := $(MEMLEAK_BCC_DIR)/memleak_dwunw_user.c
MEMLEAK_BCC_TRACE_HELPERS := $(MEMLEAK_BCC_DIR)/trace_helpers.c
MEMLEAK_BCC_UPROBE_HELPERS := $(MEMLEAK_BCC_DIR)/uprobe_helpers.c
MEMLEAK_BCC_TARGET := $(BUILD_ROOT)/examples/memleak_bcc_dwunw/memleak_dwunw_user
MEMLEAK_BCC_BPF_SRC := $(MEMLEAK_BCC_DIR)/memleak_dwunw.bpf.c
MEMLEAK_BCC_BPF_OBJ := $(BUILD_ROOT)/examples/memleak_bcc_dwunw/memleak_dwunw.bpf.o
MEMLEAK_BCC_SKEL := $(BUILD_ROOT)/examples/memleak_bcc_dwunw/memleak_dwunw.skel.h
EXAMPLE_BINS := $(EXAMPLE_MEMLEAK_TARGET) $(MEMLEAK_BCC_TARGET)

CORE_SRCS := $(wildcard $(SRC_ROOT)/core/*.c)
DWARF_SRCS := $(wildcard $(SRC_ROOT)/dwarf/*.c)
ARCH_SRCS := $(wildcard $(SRC_ROOT)/arch/*/*.c)
UNWINDER_SRCS := $(wildcard $(SRC_ROOT)/unwinder/*.c)
UTIL_SRCS := $(wildcard $(SRC_ROOT)/utils/*.c)
SRCS := $(CORE_SRCS) $(DWARF_SRCS) $(ARCH_SRCS) $(UNWINDER_SRCS) $(UTIL_SRCS)

OBJS := $(patsubst $(SRC_ROOT)/%.c,$(OBJ_ROOT)/%.o,$(SRCS))

TEST_SRCS := $(wildcard tests/unit/*.c)
TEST_BINS := $(patsubst tests/unit/%.c,$(BUILD_ROOT)/tests/%,$(TEST_SRCS))
INTEGRATION_SRCS := $(wildcard tests/integration/*.c)
INTEGRATION_BINS := $(patsubst tests/integration/%.c,$(BUILD_ROOT)/tests/integration/%,$(INTEGRATION_SRCS))

CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
CFLAGS += -fPIC -ffunction-sections -fdata-sections
CFLAGS += -I$(INCLUDE_ROOT)
CFLAGS += -Isrc
CFLAGS += -Iexamples/bpf_memleak
CFLAGS += $(DWUNW_ARCH_CFLAGS)
EXAMPLE_CFLAGS := $(filter-out -pedantic,$(CFLAGS))
LDFLAGS ?=
HOST_CC ?= cc
LIBBPF_CFLAGS ?=
LIBBPF_LDLIBS ?= -lbpf -lelf -lz
BPF_CLANG ?= clang
BPF_CFLAGS ?= -target bpf -D__TARGET_ARCH_x86 -O2 -g -Wall -Werror
BPF_CFLAGS += -I$(MEMLEAK_BCC_DIR) $(LIBBPF_CFLAGS)
BPFTOOL ?= bpftool

.PHONY: all clean print-config help test unit examples

all: $(LIB_TARGET)

test: all $(TEST_FIXTURE) $(TEST_BINS) $(INTEGRATION_BINS)
	@set -e; for t in $(TEST_BINS); do \
		echo "[RUN] $$t"; \
		DWUNW_TEST_FIXTURE=$(TEST_FIXTURE) "$$t"; \
	done; \
	for t in $(INTEGRATION_BINS); do \
		echo "[RUN] $$t"; \
		DWUNW_TEST_FIXTURE=$(TEST_FIXTURE) "$$t"; \
	done

unit: test

examples: $(EXAMPLE_BINS)

$(LIB_TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(OBJ_ROOT)/%.o: $(SRC_ROOT)/%.c | $(OBJ_ROOT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_ROOT):
	@mkdir -p $(OBJ_ROOT)

$(BUILD_ROOT)/tests/%: tests/unit/%.c $(LIB_TARGET)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(LIB_TARGET) -o $@

$(BUILD_ROOT)/tests/integration/%: tests/integration/%.c $(LIB_TARGET)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(LIB_TARGET) -o $@

$(TEST_FIXTURE): tests/fixtures/dwarf_fixture.c
	@mkdir -p $(dir $@)
	$(HOST_CC) -g -O0 $< -o $@

$(EXAMPLE_MEMLEAK_TARGET): $(EXAMPLE_MEMLEAK_SRC) $(LIB_TARGET) examples/bpf_memleak/memleak_events.h
	@mkdir -p $(dir $@)
	$(HOST_CC) $(EXAMPLE_CFLAGS) $(LIBBPF_CFLAGS) -Iexamples/bpf_memleak \
		$< $(LIB_TARGET) $(LIBBPF_LDLIBS) -o $@

$(MEMLEAK_BCC_TARGET): $(MEMLEAK_BCC_USER_SRC) $(MEMLEAK_BCC_TRACE_HELPERS) $(MEMLEAK_BCC_UPROBE_HELPERS) $(LIB_TARGET) $(MEMLEAK_BCC_SKEL)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(EXAMPLE_CFLAGS) $(LIBBPF_CFLAGS) -I$(MEMLEAK_BCC_DIR) \
		-I$(BUILD_ROOT)/examples/memleak_bcc_dwunw \
		$< $(MEMLEAK_BCC_TRACE_HELPERS) $(MEMLEAK_BCC_UPROBE_HELPERS) $(LIB_TARGET) $(LIBBPF_LDLIBS) -lpthread -lelf -lz \
		-o $@

$(MEMLEAK_BCC_BPF_OBJ): $(MEMLEAK_BCC_BPF_SRC) $(MEMLEAK_BCC_DIR)/vmlinux.h $(MEMLEAK_BCC_DIR)/memleak.h $(MEMLEAK_BCC_DIR)/maps.bpf.h $(MEMLEAK_BCC_DIR)/core_fixes.bpf.h $(MEMLEAK_BCC_DIR)/memleak_dwunw_events.h
	@mkdir -p $(dir $@)
	$(BPF_CLANG) $(BPF_CFLAGS) -c $< -o $@

$(MEMLEAK_BCC_SKEL): $(MEMLEAK_BCC_BPF_OBJ)
	@mkdir -p $(dir $@)
	$(BPFTOOL) gen skeleton $< > $@

clean:
	rm -rf build

print-config:
	@echo "ARCH=$(ARCH)"
	@echo "CC=$(CC)"
	@echo "AR=$(AR)"
	@echo "CROSS_COMPILE=$(CROSS_COMPILE)"
	@echo "DWUNW_ARCH_CFLAGS=$(DWUNW_ARCH_CFLAGS)"

help:
	@echo "Targets:"
	@echo "  all            Build libdwunw.a for ARCH=$(ARCH)"
	@echo "  test           Build and run unit tests for ARCH=$(ARCH)"
	@echo "  examples       Build example binaries (memleak_user)"
	@echo "  clean          Remove build artifacts"
	@echo "  print-config   Show the resolved toolchain settings"
	@echo "Variables:"
	@echo "  ARCH           One of x86_64, arm64, mips32 (default x86_64)"
	@echo "  BUILD_ROOT     Override output directory (default build/ARCH)"
	@echo "  CROSS_COMPILE  Override compiler prefix"
	@echo "  HOST_CC        Native compiler used for test fixtures (default cc)"
	@echo "  LIBBPF_CFLAGS  Extra cflags for libbpf-enabled examples"
	@echo "  LIBBPF_LDLIBS  Extra libs for libbpf-enabled examples"
