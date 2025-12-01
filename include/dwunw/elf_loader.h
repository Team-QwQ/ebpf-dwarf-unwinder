#ifndef DWUNW_ELF_LOADER_H
#define DWUNW_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#include "dwunw/config.h"
#include "dwunw/dwarf_sections.h"
#include "dwunw/status.h"

struct dwunw_elf_handle {
    char path[DWUNW_MAX_PATH_LEN];
    void *image;
    size_t size;
    uint8_t elf_class;
    uint8_t elf_data;
    size_t shoff;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
    const uint8_t *shstrtab;
};

dwunw_status_t dwunw_elf_open(const char *path, struct dwunw_elf_handle *out);
void dwunw_elf_close(struct dwunw_elf_handle *handle);

dwunw_status_t dwunw_elf_get_section(const struct dwunw_elf_handle *handle,
                                     const char *name,
                                     struct dwunw_dwarf_section *out);

dwunw_status_t dwunw_elf_collect_dwarf(const struct dwunw_elf_handle *handle,
                                       struct dwunw_dwarf_sections *sections);

#endif /* DWUNW_ELF_LOADER_H */
