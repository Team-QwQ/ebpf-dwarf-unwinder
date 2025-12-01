#ifndef DWUNW_DWARF_SECTIONS_H
#define DWUNW_DWARF_SECTIONS_H

#include <stddef.h>
#include <stdint.h>

struct dwunw_dwarf_section {
    const uint8_t *data;
    size_t size;
};

struct dwunw_dwarf_sections {
    struct dwunw_dwarf_section debug_info;
    struct dwunw_dwarf_section debug_frame;
    struct dwunw_dwarf_section eh_frame;
};

#endif /* DWUNW_DWARF_SECTIONS_H */
