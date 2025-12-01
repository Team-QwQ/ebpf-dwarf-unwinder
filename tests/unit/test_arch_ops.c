#include <assert.h>
#include <stdio.h>

#include "dwunw/arch_ops.h"

static void
test_x86_64_ops(void)
{
    struct dwunw_regset regs;
    struct dwunw_frame_window win;
    const struct dwunw_arch_ops *ops;
    uint64_t value;

    assert(dwunw_regset_prepare(&regs, DWUNW_ARCH_X86_64) == DWUNW_OK);
    regs.sp = 0x1000;
    regs.pc = 0x2000;

    ops = dwunw_arch_resolve(DWUNW_ARCH_X86_64);
    assert(ops && ops->arch == DWUNW_ARCH_X86_64);

    assert(ops->compute_cfa(&regs, &value) == DWUNW_OK);
    assert(value == 0x1000);

    assert(ops->read_return_addr(&regs, &value) == DWUNW_OK);
    assert(value == 0x2000);

    assert(ops->open_frame(&regs, &win) == DWUNW_OK);
    assert(win.cfa == 0x1000 && win.ra == 0x2000);
}

static void
test_arm64_ops(void)
{
    struct dwunw_regset regs;
    struct dwunw_frame_window win;
    const struct dwunw_arch_ops *ops;
    uint64_t value;

    assert(dwunw_regset_prepare(&regs, DWUNW_ARCH_ARM64) == DWUNW_OK);
    regs.sp = 0x1100;
    regs.pc = 0x2200;
    regs.regs[29] = 0x3300;
    regs.regs[30] = 0x4400;

    ops = dwunw_arch_resolve(DWUNW_ARCH_ARM64);
    assert(ops && ops->arch == DWUNW_ARCH_ARM64);

    assert(ops->compute_cfa(&regs, &value) == DWUNW_OK);
    assert(value == 0x3300 + 16);

    assert(ops->read_return_addr(&regs, &value) == DWUNW_OK);
    assert(value == 0x4400);

    assert(ops->open_frame(&regs, &win) == DWUNW_OK);
    assert(win.cfa == 0x3300 + 16 && win.ra == 0x4400);

    regs.regs[29] = 0;
    regs.regs[30] = 0;
    assert(ops->compute_cfa(&regs, &value) == DWUNW_OK);
    assert(value == regs.sp);
    assert(ops->read_return_addr(&regs, &value) == DWUNW_OK);
    assert(value == regs.pc);
}

static void
test_mips32_ops(void)
{
    struct dwunw_regset regs;
    struct dwunw_frame_window win;
    const struct dwunw_arch_ops *ops;
    uint64_t value;

    assert(dwunw_regset_prepare(&regs, DWUNW_ARCH_MIPS32) == DWUNW_OK);
    regs.sp = 0x1500;
    regs.pc = 0x2500;
    regs.regs[30] = 0x3500;
    regs.regs[31] = 0x4500;

    ops = dwunw_arch_resolve(DWUNW_ARCH_MIPS32);
    assert(ops && ops->arch == DWUNW_ARCH_MIPS32);

    assert(ops->compute_cfa(&regs, &value) == DWUNW_OK);
    assert(value == 0x3500 + 8);

    assert(ops->read_return_addr(&regs, &value) == DWUNW_OK);
    assert(value == 0x4500);

    assert(ops->open_frame(&regs, &win) == DWUNW_OK);
    assert(win.cfa == 0x3500 + 8 && win.ra == 0x4500);

    regs.regs[30] = 0;
    regs.regs[31] = 0;
    assert(ops->compute_cfa(&regs, &value) == DWUNW_OK);
    assert(value == regs.sp);
    assert(ops->read_return_addr(&regs, &value) == DWUNW_OK);
    assert(value == regs.pc);
}

int
main(void)
{
    test_x86_64_ops();
    test_arm64_ops();
    test_mips32_ops();

    puts("arch_ops: ok");
    return 0;
}
