#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwunw/elf_loader.h"
#include "dwunw/module_cache.h"

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

int
main(void)
{
    test_loader_invalid_path();
    test_loader_valid_fixture();
    test_module_cache_basic();
    puts("loader: ok");
    return 0;
}
