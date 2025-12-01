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
test_stub_ops(enum dwunw_arch_id arch)
{
    struct dwunw_regset regs;
    const struct dwunw_arch_ops *ops;
    uint64_t value;
    struct dwunw_frame_window win;

    assert(dwunw_regset_prepare(&regs, arch) == DWUNW_OK);
    ops = dwunw_arch_resolve(arch);
    assert(ops && ops->arch == arch);

    assert(ops->compute_cfa(&regs, &value) == DWUNW_ERR_NOT_IMPLEMENTED);
    assert(ops->read_return_addr(&regs, &value) == DWUNW_ERR_NOT_IMPLEMENTED);
    assert(ops->open_frame(&regs, &win) == DWUNW_ERR_NOT_IMPLEMENTED);
}

int
main(void)
{
    test_x86_64_ops();
    test_stub_ops(DWUNW_ARCH_ARM64);
    test_stub_ops(DWUNW_ARCH_MIPS32);

    puts("arch_ops: ok");
    return 0;
}
