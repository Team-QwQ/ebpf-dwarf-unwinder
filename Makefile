# SPDX-License-Identifier: MIT

include mk/toolchain.mk

SRC_ROOT := src
INCLUDE_ROOT := include
BUILD_ROOT ?= build/$(ARCH)
OBJ_ROOT := $(BUILD_ROOT)/obj
LIB_TARGET := $(BUILD_ROOT)/libdwunw.a
TEST_FIXTURE := $(BUILD_ROOT)/fixtures/dwarf_fixture

CORE_SRCS := $(wildcard $(SRC_ROOT)/core/*.c)
DWARF_SRCS := $(wildcard $(SRC_ROOT)/dwarf/*.c)
ARCH_SRCS := $(wildcard $(SRC_ROOT)/arch/*/*.c)
SRCS := $(CORE_SRCS) $(DWARF_SRCS) $(ARCH_SRCS)

OBJS := $(patsubst $(SRC_ROOT)/%.c,$(OBJ_ROOT)/%.o,$(SRCS))

TEST_SRCS := $(wildcard tests/unit/*.c)
TEST_BINS := $(patsubst tests/unit/%.c,$(BUILD_ROOT)/tests/%,$(TEST_SRCS))

CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
CFLAGS += -fPIC -ffunction-sections -fdata-sections
CFLAGS += -I$(INCLUDE_ROOT)
CFLAGS += $(DWUNW_ARCH_CFLAGS)
LDFLAGS ?=
HOST_CC ?= cc

.PHONY: all clean print-config help test unit

all: $(LIB_TARGET)

test: all $(TEST_FIXTURE) $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do \
		echo "[RUN] $$t"; \
		DWUNW_TEST_FIXTURE=$(TEST_FIXTURE) "$$t"; \
	done

unit: test

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

$(TEST_FIXTURE): tests/fixtures/dwarf_fixture.c
	@mkdir -p $(dir $@)
	$(HOST_CC) -g -O0 $< -o $@

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
	@echo "  clean          Remove build artifacts"
	@echo "  print-config   Show the resolved toolchain settings"
	@echo "Variables:"
	@echo "  ARCH           One of x86_64, arm64, mips32 (default x86_64)"
	@echo "  BUILD_ROOT     Override output directory (default build/ARCH)"
	@echo "  CROSS_COMPILE  Override compiler prefix"
	@echo "  HOST_CC        Native compiler used for test fixtures (default cc)"
