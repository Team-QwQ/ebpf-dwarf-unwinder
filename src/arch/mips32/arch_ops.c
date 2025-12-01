#include "dwunw/arch_ops.h"
#include "../arch_ops_internal.h"

static dwunw_status_t
mips32_normalize(struct dwunw_regset *regs)
{
    if (!regs) {
        return DWUNW_ERR_INVALID_ARG;
    }

    regs->arch = DWUNW_ARCH_MIPS32;
    if (regs->version == 0) {
        regs->version = DWUNW_REGSET_VERSION;
    }
    return DWUNW_OK;
}

static dwunw_status_t
mips32_compute_cfa(const struct dwunw_regset *regs, uint64_t *cfa)
{
    (void)regs;
    (void)cfa;
    return DWUNW_ERR_NOT_IMPLEMENTED;
}

static dwunw_status_t
mips32_read_return_addr(const struct dwunw_regset *regs, uint64_t *ra)
{
    (void)regs;
    (void)ra;
    return DWUNW_ERR_NOT_IMPLEMENTED;
}

static dwunw_status_t
mips32_open_frame(const struct dwunw_regset *regs, struct dwunw_frame_window *win)
{
    (void)regs;
    (void)win;
    return DWUNW_ERR_NOT_IMPLEMENTED;
}

const struct dwunw_arch_ops *
dwunw_arch_ops_mips32(void)
{
    static const struct dwunw_arch_ops ops = {
        .arch = DWUNW_ARCH_MIPS32,
        .name = "mips32",
        .gpr_count = 32,
        .normalize = mips32_normalize,
        .compute_cfa = mips32_compute_cfa,
        .read_return_addr = mips32_read_return_addr,
        .open_frame = mips32_open_frame,
    };

    return &ops;
}
