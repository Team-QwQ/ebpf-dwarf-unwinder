#ifndef DWUNW_DWARF_INDEX_H
#define DWUNW_DWARF_INDEX_H

#include "dwunw/dwarf_sections.h"
#include "dwunw/elf_loader.h"
#include "dwunw/status.h"

struct dwunw_dwarf_index {
    struct dwunw_dwarf_sections sections;
    uint32_t flags;
};

dwunw_status_t dwunw_dwarf_index_init(struct dwunw_dwarf_index *index,
                                      const struct dwunw_elf_handle *handle);
void dwunw_dwarf_index_reset(struct dwunw_dwarf_index *index);

#endif /* DWUNW_DWARF_INDEX_H */
