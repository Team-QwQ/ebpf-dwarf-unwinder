#ifndef DWUNW_MODULE_CACHE_H
#define DWUNW_MODULE_CACHE_H

#include "dwunw/config.h"
#include "dwunw/dwarf_index.h"
#include "dwunw/elf_loader.h"
#include "dwunw/status.h"

struct dwunw_module_handle {
    struct dwunw_elf_handle elf;
    struct dwunw_dwarf_index index;
    uint32_t flags;
};

enum dwunw_module_slot_state {
    DWUNW_MODULE_SLOT_UNUSED = 0,
    DWUNW_MODULE_SLOT_ACTIVE = 1,
    DWUNW_MODULE_SLOT_WARM   = 2,
};

struct dwunw_module_cache_entry {
    char path[DWUNW_MAX_PATH_LEN];
    struct dwunw_module_handle handle;
    uint32_t refcnt;
    uint8_t state;
    uint64_t warm_seq;
};

struct dwunw_module_cache {
    struct dwunw_module_cache_entry entries[DWUNW_MODULE_CACHE_CAPACITY];
    uint64_t warm_clock;
};

void dwunw_module_cache_init(struct dwunw_module_cache *cache);
void dwunw_module_cache_flush(struct dwunw_module_cache *cache);

dwunw_status_t dwunw_module_cache_acquire(struct dwunw_module_cache *cache,
                                          const char *path,
                                          struct dwunw_module_handle **handle_out);

dwunw_status_t dwunw_module_cache_release(struct dwunw_module_cache *cache,
                                          struct dwunw_module_handle *handle);

#endif /* DWUNW_MODULE_CACHE_H */
