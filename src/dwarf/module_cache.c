#include <stddef.h>
#include <string.h>

#include "dwunw/module_cache.h"

static struct dwunw_module_cache_entry *
dwunw_module_cache_find(struct dwunw_module_cache *cache, const char *path)
{
    size_t i;

    for (i = 0; i < DWUNW_MODULE_CACHE_CAPACITY; ++i) {
        struct dwunw_module_cache_entry *entry = &cache->entries[i];
        if (!entry->in_use) {
            continue;
        }
        if (strncmp(entry->path, path, DWUNW_MAX_PATH_LEN) == 0) {
            return entry;
        }
    }

    return NULL;
}

static struct dwunw_module_cache_entry *
dwunw_module_cache_alloc(struct dwunw_module_cache *cache)
{
    size_t i;

    for (i = 0; i < DWUNW_MODULE_CACHE_CAPACITY; ++i) {
        struct dwunw_module_cache_entry *entry = &cache->entries[i];
        if (!entry->in_use) {
            return entry;
        }
    }

    return NULL;
}

void
dwunw_module_cache_init(struct dwunw_module_cache *cache)
{
    if (!cache) {
        return;
    }

    memset(cache, 0, sizeof(*cache));
}

void
dwunw_module_cache_flush(struct dwunw_module_cache *cache)
{
    size_t i;

    if (!cache) {
        return;
    }

    for (i = 0; i < DWUNW_MODULE_CACHE_CAPACITY; ++i) {
        struct dwunw_module_cache_entry *entry = &cache->entries[i];
        if (!entry->in_use) {
            continue;
        }
        dwunw_elf_close(&entry->handle.elf);
        dwunw_dwarf_index_reset(&entry->handle.index);
        memset(entry->path, 0, sizeof(entry->path));
        entry->refcnt = 0;
        entry->in_use = 0;
    }
}

dwunw_status_t
dwunw_module_cache_acquire(struct dwunw_module_cache *cache,
                          const char *path,
                          struct dwunw_module_handle **handle_out)
{
    struct dwunw_module_cache_entry *entry;
    dwunw_status_t status;

    if (!cache || !path || !handle_out) {
        return DWUNW_ERR_INVALID_ARG;
    }

    entry = dwunw_module_cache_find(cache, path);
    if (entry) {
        entry->refcnt++;
        *handle_out = &entry->handle;
        return DWUNW_OK;
    }

    entry = dwunw_module_cache_alloc(cache);
    if (!entry) {
        return DWUNW_ERR_CACHE_FULL;
    }

    status = dwunw_elf_open(path, &entry->handle.elf);
    if (status != DWUNW_OK) {
        return status;
    }

    status = dwunw_dwarf_index_init(&entry->handle.index, &entry->handle.elf);
    if (status != DWUNW_OK) {
        dwunw_elf_close(&entry->handle.elf);
        memset(&entry->handle, 0, sizeof(entry->handle));
        return status;
    }

    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->refcnt = 1;
    entry->in_use = 1;
    *handle_out = &entry->handle;
    return DWUNW_OK;
}

dwunw_status_t
dwunw_module_cache_release(struct dwunw_module_cache *cache,
                          struct dwunw_module_handle *handle)
{
    size_t i;

    if (!cache || !handle) {
        return DWUNW_ERR_INVALID_ARG;
    }

    for (i = 0; i < DWUNW_MODULE_CACHE_CAPACITY; ++i) {
        struct dwunw_module_cache_entry *entry = &cache->entries[i];
        if (!entry->in_use) {
            continue;
        }
        if (&entry->handle != handle) {
            continue;
        }

        if (entry->refcnt == 0) {
            return DWUNW_ERR_INVALID_ARG;
        }

        entry->refcnt--;
        if (entry->refcnt == 0) {
            dwunw_elf_close(&entry->handle.elf);
            dwunw_dwarf_index_reset(&entry->handle.index);
            memset(entry->path, 0, sizeof(entry->path));
            entry->in_use = 0;
        }

        return DWUNW_OK;
    }

    return DWUNW_ERR_INVALID_ARG;
}
