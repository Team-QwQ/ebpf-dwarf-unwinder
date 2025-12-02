#define _POSIX_C_SOURCE 200809L
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "dwunw/elf_loader.h"

/* Pull the entire ELF image into memory so later section slices are
 * O(1) pointer math instead of repeat syscalls. */
static dwunw_status_t
dwunw_elf_load_image(struct dwunw_elf_handle *handle, int fd, size_t size)
{
    ssize_t read_ret;

    handle->image = malloc(size);
    if (!handle->image) {
        return DWUNW_ERR_IO;
    }

    read_ret = pread(fd, handle->image, size, 0);
    if (read_ret < 0 || (size_t)read_ret != size) {
        free(handle->image);
        handle->image = NULL;
        return DWUNW_ERR_IO;
    }

    handle->size = size;
    return DWUNW_OK;
}

/* Only parse enough of the ELF header to discover the section table layout. */
static dwunw_status_t
dwunw_elf_parse_headers(struct dwunw_elf_handle *handle)
{
    const uint8_t *image = handle->image;

    if (handle->size < EI_NIDENT) {
        return DWUNW_ERR_BAD_FORMAT;
    }

    if (memcmp(image, ELFMAG, SELFMAG) != 0) {
        return DWUNW_ERR_BAD_FORMAT;
    }

    handle->elf_class = image[EI_CLASS];
    handle->elf_data = image[EI_DATA];

    if (handle->elf_data != ELFDATA2LSB) {
        return DWUNW_ERR_BAD_FORMAT;
    }

    if (handle->elf_class == ELFCLASS64) {
        if (handle->size < sizeof(Elf64_Ehdr)) {
            return DWUNW_ERR_BAD_FORMAT;
        }
        const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
        handle->shoff = eh->e_shoff;
        handle->shentsize = eh->e_shentsize;
        handle->shnum = eh->e_shnum;
        handle->shstrndx = eh->e_shstrndx;
    } else if (handle->elf_class == ELFCLASS32) {
        if (handle->size < sizeof(Elf32_Ehdr)) {
            return DWUNW_ERR_BAD_FORMAT;
        }
        const Elf32_Ehdr *eh = (const Elf32_Ehdr *)image;
        handle->shoff = eh->e_shoff;
        handle->shentsize = eh->e_shentsize;
        handle->shnum = eh->e_shnum;
        handle->shstrndx = eh->e_shstrndx;
    } else {
        return DWUNW_ERR_BAD_FORMAT;
    }

    if (handle->shoff == 0 || handle->shentsize == 0 || handle->shnum == 0) {
        return DWUNW_ERR_BAD_FORMAT;
    }

    return DWUNW_OK;
}

/* Helper to grab a section header with strict bounds checking. */
static const uint8_t *
dwunw_elf_section_header(const struct dwunw_elf_handle *handle, uint16_t index)
{
    size_t offset = handle->shoff + (size_t)index * handle->shentsize;

    if (offset + handle->shentsize > handle->size) {
        return NULL;
    }

    return (const uint8_t *)handle->image + offset;
}

/* Section-name lookup is deferred until here so openers stay lightweight. */
static dwunw_status_t
dwunw_elf_locate_shstrtab(struct dwunw_elf_handle *handle)
{
    const uint8_t *sh;

    if (handle->shstrndx >= handle->shnum) {
        return DWUNW_ERR_BAD_FORMAT;
    }

    sh = dwunw_elf_section_header(handle, handle->shstrndx);
    if (!sh) {
        return DWUNW_ERR_BAD_FORMAT;
    }

    if (handle->elf_class == ELFCLASS64) {
        const Elf64_Shdr *hdr = (const Elf64_Shdr *)sh;
        if (hdr->sh_offset + hdr->sh_size > handle->size) {
            return DWUNW_ERR_BAD_FORMAT;
        }
        handle->shstrtab = (const uint8_t *)handle->image + hdr->sh_offset;
    } else {
        const Elf32_Shdr *hdr = (const Elf32_Shdr *)sh;
        if ((uint64_t)hdr->sh_offset + hdr->sh_size > handle->size) {
            return DWUNW_ERR_BAD_FORMAT;
        }
        handle->shstrtab = (const uint8_t *)handle->image + hdr->sh_offset;
    }

    return DWUNW_OK;
}

static dwunw_status_t
dwunw_elf_initialize(struct dwunw_elf_handle *handle)
{
    dwunw_status_t status;

    status = dwunw_elf_parse_headers(handle);
    if (status != DWUNW_OK) {
        return status;
    }

    status = dwunw_elf_locate_shstrtab(handle);
    if (status != DWUNW_OK) {
        return status;
    }

    return DWUNW_OK;
}

dwunw_status_t
dwunw_elf_open(const char *path, struct dwunw_elf_handle *out)
{
    struct stat st;
    int fd;
    dwunw_status_t status;

    if (!path || !out) {
        return DWUNW_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return DWUNW_ERR_IO;
    }

    if (fstat(fd, &st) < 0) {
        close(fd);
        return DWUNW_ERR_IO;
    }

    if ((size_t)st.st_size == 0) {
        close(fd);
        return DWUNW_ERR_BAD_FORMAT;
    }

    status = dwunw_elf_load_image(out, fd, (size_t)st.st_size);
    if (status != DWUNW_OK) {
        close(fd);
        return status;
    }

    status = dwunw_elf_initialize(out);
    if (status != DWUNW_OK) {
        dwunw_elf_close(out);
        close(fd);
        return status;
    }

    strncpy(out->path, path, sizeof(out->path) - 1);
    close(fd);
    return DWUNW_OK;
}

void
dwunw_elf_close(struct dwunw_elf_handle *handle)
{
    if (!handle) {
        return;
    }

    if (handle->image) {
        free(handle->image);
    }

    memset(handle, 0, sizeof(*handle));
}

/* Convert an ELF section header into a DWARF slice pointing inside the image. */
static dwunw_status_t
dwunw_elf_section_slice(const struct dwunw_elf_handle *handle,
                      struct dwunw_dwarf_section *out,
                      const uint8_t *sh)
{
    out->data = NULL;
    out->size = 0;

    if (handle->elf_class == ELFCLASS64) {
        const Elf64_Shdr *hdr = (const Elf64_Shdr *)sh;
        if (hdr->sh_offset + hdr->sh_size > handle->size) {
            return DWUNW_ERR_BAD_FORMAT;
        }
        out->data = (const uint8_t *)handle->image + hdr->sh_offset;
        out->size = (size_t)hdr->sh_size;
    } else {
        const Elf32_Shdr *hdr = (const Elf32_Shdr *)sh;
        if ((uint64_t)hdr->sh_offset + hdr->sh_size > handle->size) {
            return DWUNW_ERR_BAD_FORMAT;
        }
        out->data = (const uint8_t *)handle->image + hdr->sh_offset;
        out->size = (size_t)hdr->sh_size;
    }

    return DWUNW_OK;
}

dwunw_status_t
dwunw_elf_get_section(const struct dwunw_elf_handle *handle,
                      const char *name,
                      struct dwunw_dwarf_section *out)
{
    uint16_t i;

    if (!handle || !name || !out) {
        return DWUNW_ERR_INVALID_ARG;
    }

    for (i = 0; i < handle->shnum; ++i) {
        const uint8_t *sh = dwunw_elf_section_header(handle, i);
        const char *sh_name;
        dwunw_status_t status;

        if (!sh) {
            continue;
        }

        if (handle->elf_class == ELFCLASS64) {
            const Elf64_Shdr *hdr = (const Elf64_Shdr *)sh;
            sh_name = (const char *)handle->shstrtab + hdr->sh_name;
        } else {
            const Elf32_Shdr *hdr = (const Elf32_Shdr *)sh;
            sh_name = (const char *)handle->shstrtab + hdr->sh_name;
        }

        if (strcmp(sh_name, name) != 0) {
            continue;
        }

        status = dwunw_elf_section_slice(handle, out, sh);
        if (status != DWUNW_OK) {
            return status;
        }

        return DWUNW_OK;
    }

    return DWUNW_ERR_NO_DEBUG_DATA;
}

dwunw_status_t
dwunw_elf_collect_dwarf(const struct dwunw_elf_handle *handle,
                        struct dwunw_dwarf_sections *sections)
{
    dwunw_status_t status;

    if (!sections) {
        return DWUNW_ERR_INVALID_ARG;
    }

    memset(sections, 0, sizeof(*sections));

    /* .debug_info is the minimum required payload for any DWARF walk. */
    status = dwunw_elf_get_section(handle, ".debug_info", &sections->debug_info);
    if (status != DWUNW_OK) {
        return status;
    }

    /* The frame sections are optional; normalize missing ones to an empty
     * descriptor so upstream callers do not need special cases. */
    status = dwunw_elf_get_section(handle, ".debug_frame", &sections->debug_frame);
    if (status == DWUNW_ERR_NO_DEBUG_DATA) {
        memset(&sections->debug_frame, 0, sizeof(sections->debug_frame));
    } else if (status != DWUNW_OK) {
        return status;
    }

    status = dwunw_elf_get_section(handle, ".eh_frame", &sections->eh_frame);
    if (status == DWUNW_ERR_NO_DEBUG_DATA) {
        memset(&sections->eh_frame, 0, sizeof(sections->eh_frame));
    } else if (status != DWUNW_OK) {
        return status;
    }

    return DWUNW_OK;
}
