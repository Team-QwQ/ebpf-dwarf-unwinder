#ifndef DWUNW_DWARF_INDEX_H
#define DWUNW_DWARF_INDEX_H

#include "dwunw/dwarf_sections.h"
#include "dwunw/elf_loader.h"
#include "dwunw/status.h"

struct dwunw_cie_record;
struct dwunw_fde_record;

struct dwunw_dwarf_index {
    struct dwunw_dwarf_sections sections;
    struct dwunw_cie_record *cies;
    size_t cie_count;
    struct dwunw_fde_record *fdes;
    size_t fde_count;
    uint32_t flags;
};

dwunw_status_t dwunw_dwarf_index_init(struct dwunw_dwarf_index *index,
                                      const struct dwunw_elf_handle *handle);
void dwunw_dwarf_index_reset(struct dwunw_dwarf_index *index);

#endif /* DWUNW_DWARF_INDEX_H */
