#pragma once

#include <stddef.h>
#include <stdint.h>

#include "dwunw/dwarf_sections.h"
#include "dwunw/elf_loader.h"
#include "dwunw/status.h"
#include "dwunw/unwind.h"

typedef dwunw_status_t (*dwunw_memory_read_fn)(void *ctx,
                                               uint64_t address,
                                               void *dst,
                                               size_t size);

struct dwunw_cie_record {
    uint8_t version;
    uint8_t address_size;
    uint8_t return_reg;
    uint8_t ptr_encoding;
    int64_t data_align;
    uint64_t code_align;
    uint64_t offset;
    const char *augmentation;
    size_t augmentation_len;
    const uint8_t *instructions;
    size_t instructions_size;
};

struct dwunw_fde_record {
    const struct dwunw_cie_record *cie;
    uint64_t offset;
    uint64_t pc_begin;
    uint64_t pc_range;
    const uint8_t *instructions;
    size_t instructions_size;
};

dwunw_status_t
dwunw_cfi_build(const struct dwunw_dwarf_sections *sections,
                struct dwunw_cie_record **cies_out,
                size_t *cie_count,
                struct dwunw_fde_record **fdes_out,
                size_t *fde_count);

void
dwunw_cfi_free(struct dwunw_cie_record *cies,
               struct dwunw_fde_record *fdes);

const struct dwunw_fde_record *
dwunw_cfi_find_fde(const struct dwunw_fde_record *fdes,
                   size_t count,
                   uint64_t pc);

dwunw_status_t
dwunw_cfi_eval(const struct dwunw_fde_record *fde,
               uint64_t pc,
               struct dwunw_regset *regs,
               dwunw_memory_read_fn reader,
               void *reader_ctx,
               struct dwunw_frame *frame);