#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "dwunw/arch_ops.h"
#include "dwunw/status.h"
#include "dwunw/unwind.h"
#include "dwarf/cfi.h"

struct mock_stack {
    uint64_t base;
    uint8_t bytes[64];
};

static const uint8_t simple_debug_frame[] = {
    /* CIE */
    0x0e, 0x00, 0x00, 0x00,             /* length */
    0xff, 0xff, 0xff, 0xff,             /* CIE id */
    0x01,                               /* version */
    0x00,                               /* augmentation */
    0x01,                               /* code align */
    0x08,                               /* data align */
    0x10,                               /* return register */
    0x0c, 0x07, 0x10,                   /* def_cfa r7+16 */
    0x90, 0x01,                         /* offset r16 @ CFA+8 */
    /* FDE */
    0x14, 0x00, 0x00, 0x00,             /* length */
    0x00, 0x00, 0x00, 0x00,             /* CIE pointer */
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* initial_location 0x1000 */
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* address_range 0x40 */
};

static dwunw_status_t
mock_reader(void *ctx, uint64_t address, void *dst, size_t size)
{
    const struct mock_stack *mem = ctx;

    if (address < mem->base || address + size > mem->base + sizeof(mem->bytes)) {
        return DWUNW_ERR_INVALID_ARG;
    }

    memcpy(dst, mem->bytes + (address - mem->base), size);
    return DWUNW_OK;
}

static void
build_simple_tables(struct dwunw_cie_record **cies,
                    size_t *cie_count,
                    struct dwunw_fde_record **fdes,
                    size_t *fde_count)
{
    struct dwunw_dwarf_sections sections = {
        .debug_frame = {
            .data = simple_debug_frame,
            .size = sizeof(simple_debug_frame),
        },
    };

    assert(dwunw_cfi_build(&sections, cies, cie_count, fdes, fde_count) == DWUNW_OK);
    assert(*cie_count == 1);
    assert(*fde_count == 1);
}

static void
test_cfi_build_parses_simple_section(void)
{
    struct dwunw_cie_record *cies = NULL;
    struct dwunw_fde_record *fdes = NULL;
    size_t cie_count = 0;
    size_t fde_count = 0;

    build_simple_tables(&cies, &cie_count, &fdes, &fde_count);

    assert(cies[0].code_align == 1);
    assert(cies[0].data_align == 8);
    assert(cies[0].return_reg == 0x10);
    assert(fdes[0].pc_begin == 0x1000);
    assert(fdes[0].pc_range == 0x40);

    dwunw_cfi_free(cies, fdes);
}

static void
test_cfi_eval_reads_return_address(void)
{
    struct dwunw_cie_record *cies = NULL;
    struct dwunw_fde_record *fdes = NULL;
    size_t cie_count = 0;
    size_t fde_count = 0;
    struct dwunw_regset regs;
    struct dwunw_frame frame;
    struct mock_stack stack = {
        .base = 0x1000,
    };
    const uint64_t saved_ra = 0x5000;

    build_simple_tables(&cies, &cie_count, &fdes, &fde_count);

    assert(dwunw_regset_prepare(&regs, DWUNW_ARCH_X86_64) == DWUNW_OK);
    regs.sp = 0x1000;
    regs.pc = 0x1008;
    regs.regs[7] = regs.sp;
    memcpy(&stack.bytes[0x18], &saved_ra, sizeof(saved_ra));

    assert(dwunw_cfi_eval(&fdes[0], regs.pc, &regs, mock_reader, &stack, &frame) == DWUNW_OK);
    assert(frame.pc == saved_ra);
    assert(frame.ra == saved_ra);
    assert(frame.sp == regs.sp);
    assert(frame.sp == 0x1000 + 16);
    assert(frame.flags == 0);

    dwunw_cfi_free(cies, fdes);
}

int
main(void)
{
    test_cfi_build_parses_simple_section();
    test_cfi_eval_reads_return_address();
    puts("cfi: ok");
    return 0;
}
