#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>

#include "dwunw/dwarf_index.h"
#include "dwunw/elf_loader.h"

static const char *
get_fixture_path(void)
{
    const char *path = getenv("DWUNW_TEST_FIXTURE");
    assert(path && "DWUNW_TEST_FIXTURE env is required");
    return path;
}

static void
test_reset_zeroes_everything(void)
{
    struct dwunw_dwarf_index index;

    index.flags = 0xdeadbeef;
    index.sections.debug_info.data = (const unsigned char *)0x1;
    index.sections.debug_info.size = 99;
    index.sections.debug_frame.data = (const unsigned char *)0x2;
    index.sections.debug_frame.size = 77;
    index.sections.eh_frame.data = (const unsigned char *)0x3;
    index.sections.eh_frame.size = 55;

    dwunw_dwarf_index_reset(&index);

    assert(index.flags == 0);
    assert(index.sections.debug_info.data == NULL);
    assert(index.sections.debug_info.size == 0);
    assert(index.sections.debug_frame.data == NULL);
    assert(index.sections.debug_frame.size == 0);
    assert(index.sections.eh_frame.data == NULL);
    assert(index.sections.eh_frame.size == 0);
}

static void
test_init_with_fixture(void)
{
    struct dwunw_dwarf_index index;
    struct dwunw_elf_handle handle;
    const char *fixture = get_fixture_path();
    dwunw_status_t st;

    st = dwunw_elf_open(fixture, &handle);
    assert(st == DWUNW_OK);

    st = dwunw_dwarf_index_init(&index, &handle);
    assert(st == DWUNW_OK || st == DWUNW_ERR_NO_DEBUG_DATA);

    if (st == DWUNW_OK) {
        assert(index.sections.debug_info.data != NULL);
        assert(index.sections.debug_info.size > 0);
    } else {
        assert(index.sections.debug_info.data == NULL);
        assert(index.sections.debug_info.size == 0);
    }

    dwunw_elf_close(&handle);
}

static void
test_invalid_args(void)
{
    struct dwunw_dwarf_index index;
    struct dwunw_elf_handle handle;
    const char *fixture = get_fixture_path();

    assert(dwunw_dwarf_index_init(NULL, NULL) == DWUNW_ERR_INVALID_ARG);

    assert(dwunw_elf_open(fixture, &handle) == DWUNW_OK);
    assert(dwunw_dwarf_index_init(&index, NULL) == DWUNW_ERR_INVALID_ARG);
    dwunw_elf_close(&handle);

    assert(dwunw_dwarf_index_init(NULL, &handle) == DWUNW_ERR_INVALID_ARG);
}

int
main(void)
{
    test_reset_zeroes_everything();
    test_init_with_fixture();
    test_invalid_args();
    return 0;
}
