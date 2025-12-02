#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwunw/elf_loader.h"
#include "dwunw/module_cache.h"
static struct dwunw_module_cache_entry *
find_cache_entry(struct dwunw_module_cache *cache,
                 struct dwunw_module_handle *handle)
{
    size_t i;

    for (i = 0; i < DWUNW_MODULE_CACHE_CAPACITY; ++i) {
        if (&cache->entries[i].handle == handle) {
            return &cache->entries[i];
        }
    }

    return NULL;
}


static const char *
get_fixture_path(void)
{
    const char *path = getenv("DWUNW_TEST_FIXTURE");
    assert(path && "DWUNW_TEST_FIXTURE env is required");
    return path;
}

static void
test_loader_invalid_path(void)
{
    struct dwunw_elf_handle handle;
    assert(dwunw_elf_open(NULL, &handle) == DWUNW_ERR_INVALID_ARG);
    assert(dwunw_elf_open("/path/does/not/exist", &handle) == DWUNW_ERR_IO);
}

static void
test_loader_valid_fixture(void)
{
    struct dwunw_elf_handle handle;
    const char *fixture = get_fixture_path();
    dwunw_status_t st;
    struct dwunw_dwarf_sections sections;

    st = dwunw_elf_open(fixture, &handle);
    assert(st == DWUNW_OK);

    /* Depending on compiler flags, fixture may or may not carry debug data. */
    st = dwunw_elf_collect_dwarf(&handle, &sections);
    assert(st == DWUNW_OK || st == DWUNW_ERR_NO_DEBUG_DATA);

    dwunw_elf_close(&handle);
}

static void
test_module_cache_basic(void)
{
    struct dwunw_module_cache cache;
    struct dwunw_module_handle *handle_a;
    struct dwunw_module_handle *handle_b;
    const char *fixture = get_fixture_path();
    dwunw_status_t st;

    dwunw_module_cache_init(&cache);

    st = dwunw_module_cache_acquire(&cache, fixture, &handle_a);
    assert(st == DWUNW_OK || st == DWUNW_ERR_NO_DEBUG_DATA);
    if (st != DWUNW_OK) {
        /* Skip the cache validation when debug data is absent. */
        return;
    }

    st = dwunw_module_cache_acquire(&cache, fixture, &handle_b);
    assert(st == DWUNW_OK);
    assert(handle_a == handle_b);

    assert(dwunw_module_cache_release(&cache, handle_a) == DWUNW_OK);
    assert(dwunw_module_cache_release(&cache, handle_b) == DWUNW_OK);
}

static void
test_module_cache_warm_reuse(void)
{
    struct dwunw_module_cache cache;
    struct dwunw_module_handle *handle_a;
    struct dwunw_module_handle *handle_b;
    struct dwunw_module_cache_entry *entry;
    const char *fixture = get_fixture_path();
    dwunw_status_t st;

    dwunw_module_cache_init(&cache);

    st = dwunw_module_cache_acquire(&cache, fixture, &handle_a);
    assert(st == DWUNW_OK || st == DWUNW_ERR_NO_DEBUG_DATA);
    if (st != DWUNW_OK) {
        return;
    }

    entry = find_cache_entry(&cache, handle_a);
    assert(entry != NULL);

    assert(dwunw_module_cache_release(&cache, handle_a) == DWUNW_OK);
    assert(entry->state == DWUNW_MODULE_SLOT_WARM);
    assert(entry->refcnt == 0);

    st = dwunw_module_cache_acquire(&cache, fixture, &handle_b);
    assert(st == DWUNW_OK);
    assert(handle_a == handle_b);
    assert(entry->state == DWUNW_MODULE_SLOT_ACTIVE);
    assert(entry->refcnt == 1);

    assert(dwunw_module_cache_release(&cache, handle_b) == DWUNW_OK);
}

static void
test_module_cache_warm_eviction(void)
{
    struct dwunw_module_cache cache;
    struct dwunw_module_handle *handle;
    const char *fixture = get_fixture_path();
    dwunw_status_t st;
    size_t i;

    dwunw_module_cache_init(&cache);

    /* Pretend every slot already holds a warm entry so acquire() must evict
     * the oldest one before loading the real module. */
    for (i = 0; i < DWUNW_MODULE_CACHE_CAPACITY; ++i) {
        struct dwunw_module_cache_entry *entry = &cache.entries[i];
        snprintf(entry->path, sizeof(entry->path), "dummy-%zu", i);
        entry->state = DWUNW_MODULE_SLOT_WARM;
        entry->warm_seq = (uint64_t)(i + 1);
        entry->refcnt = 0;
    }
    cache.warm_clock = DWUNW_MODULE_CACHE_CAPACITY;

    st = dwunw_module_cache_acquire(&cache, fixture, &handle);
    assert(st == DWUNW_OK || st == DWUNW_ERR_NO_DEBUG_DATA);
    if (st != DWUNW_OK) {
        return;
    }

    /* Oldest warm entry (warm_seq == 1) must now host the fixture module. */
    assert(strncmp(cache.entries[0].path, fixture, sizeof(cache.entries[0].path)) == 0);
    assert(cache.entries[0].state == DWUNW_MODULE_SLOT_ACTIVE);

    assert(dwunw_module_cache_release(&cache, handle) == DWUNW_OK);
    assert(cache.entries[0].state == DWUNW_MODULE_SLOT_WARM);
    assert(cache.entries[0].warm_seq == cache.warm_clock);
}

int
main(void)
{
    test_loader_invalid_path();
    test_loader_valid_fixture();
    test_module_cache_basic();
    test_module_cache_warm_reuse();
    test_module_cache_warm_eviction();
    puts("loader: ok");
    return 0;
}
