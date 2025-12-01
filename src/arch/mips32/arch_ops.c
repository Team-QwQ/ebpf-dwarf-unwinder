#include <stddef.h>

#include "dwunw/arch_ops.h"
#include "../arch_ops_internal.h"

#define MIPS32_REG_FP 30u
#define MIPS32_REG_RA 31u
#define MIPS32_FRAME_RECORD_SIZE 8u

static int
mips32_valid_reg(size_t idx)
{
    return idx < DWUNW_REGSET_SLOTS;
}

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
    if (!regs || !cfa) {
        return DWUNW_ERR_INVALID_ARG;
    }

    if (mips32_valid_reg(MIPS32_REG_FP)) {
        uint64_t fp = regs->regs[MIPS32_REG_FP];
        if (fp != 0) {
            *cfa = fp + MIPS32_FRAME_RECORD_SIZE;
            return DWUNW_OK;
        }
    }

    *cfa = regs->sp;
    return DWUNW_OK;
}

static dwunw_status_t
mips32_read_return_addr(const struct dwunw_regset *regs, uint64_t *ra)
{
    if (!regs || !ra) {
        return DWUNW_ERR_INVALID_ARG;
    }

    if (!mips32_valid_reg(MIPS32_REG_RA)) {
        return DWUNW_ERR_INVALID_ARG;
    }

    *ra = regs->regs[MIPS32_REG_RA];
    if (*ra == 0) {
        *ra = regs->pc;
    }

    return DWUNW_OK;
}

static dwunw_status_t
mips32_open_frame(const struct dwunw_regset *regs, struct dwunw_frame_window *win)
{
    dwunw_status_t st;

    if (!regs || !win) {
        return DWUNW_ERR_INVALID_ARG;
    }

    st = mips32_compute_cfa(regs, &win->cfa);
    if (st != DWUNW_OK) {
        return st;
    }

    return mips32_read_return_addr(regs, &win->ra);
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
