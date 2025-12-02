#include <stddef.h>
#include <string.h>

#include "dwunw/module_cache.h"

static void
dwunw_module_cache_entry_reset(struct dwunw_module_cache_entry *entry)
{
    dwunw_elf_close(&entry->handle.elf);
    dwunw_dwarf_index_reset(&entry->handle.index);
    memset(entry->path, 0, sizeof(entry->path));
    entry->refcnt = 0;
    entry->state = DWUNW_MODULE_SLOT_UNUSED;
    entry->warm_seq = 0;
}

/* Linear probe over the fixed-size cache; capacity is tiny (16 entries) so
 * a scan keeps the code simple and deterministic. */
static struct dwunw_module_cache_entry *
dwunw_module_cache_find(struct dwunw_module_cache *cache, const char *path)
{
    size_t i;

    for (i = 0; i < DWUNW_MODULE_CACHE_CAPACITY; ++i) {
        struct dwunw_module_cache_entry *entry = &cache->entries[i];
        if (entry->state == DWUNW_MODULE_SLOT_UNUSED) {
            continue;
        }
        if (strncmp(entry->path, path, DWUNW_MAX_PATH_LEN) == 0) {
            return entry;
        }
    }

    return NULL;
}

/* First look for an unused slot; otherwise evict the oldest warm slot. */
static struct dwunw_module_cache_entry *
dwunw_module_cache_alloc(struct dwunw_module_cache *cache)
{
    size_t i;
    struct dwunw_module_cache_entry *victim = NULL;

    for (i = 0; i < DWUNW_MODULE_CACHE_CAPACITY; ++i) {
        struct dwunw_module_cache_entry *entry = &cache->entries[i];
        if (entry->state == DWUNW_MODULE_SLOT_UNUSED) {
            return entry;
        }

        if (entry->state == DWUNW_MODULE_SLOT_WARM) {
            if (!victim || entry->warm_seq < victim->warm_seq) {
                victim = entry;
            }
        }
    }

    if (!victim) {
        return NULL;
    }

    dwunw_module_cache_entry_reset(victim);
    return victim;
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
        if (entry->state == DWUNW_MODULE_SLOT_UNUSED) {
            continue;
        }
        /* Release both the ELF image and parsed DWARF tables before marking
         * the slot idle. */
        dwunw_module_cache_entry_reset(entry);
    }

    cache->warm_clock = 0;
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
        if (entry->state == DWUNW_MODULE_SLOT_WARM) {
            entry->refcnt = 1;
            entry->state = DWUNW_MODULE_SLOT_ACTIVE;
            entry->warm_seq = 0;
        } else {
            /* Bump the refcount so callers must balance with _release. */
            entry->refcnt++;
        }
        *handle_out = &entry->handle;
        return DWUNW_OK;
    }

    entry = dwunw_module_cache_alloc(cache);
    if (!entry) {
        return DWUNW_ERR_CACHE_FULL;
    }

    /* Opening the ELF can still fail (permission, IO, truncation). */
    status = dwunw_elf_open(path, &entry->handle.elf);
    if (status != DWUNW_OK) {
        return status;
    }

    /* Parse DWARF metadata eagerly so future acquisitions are instant. */
    status = dwunw_dwarf_index_init(&entry->handle.index, &entry->handle.elf);
    if (status != DWUNW_OK) {
        dwunw_elf_close(&entry->handle.elf);
        memset(&entry->handle, 0, sizeof(entry->handle));
        return status;
    }

    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->refcnt = 1;
    entry->state = DWUNW_MODULE_SLOT_ACTIVE;
    entry->warm_seq = 0;
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
        if (entry->state == DWUNW_MODULE_SLOT_UNUSED) {
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
            entry->state = DWUNW_MODULE_SLOT_WARM;
            entry->warm_seq = ++cache->warm_clock;
        }

        return DWUNW_OK;
    }

    return DWUNW_ERR_INVALID_ARG;
}
