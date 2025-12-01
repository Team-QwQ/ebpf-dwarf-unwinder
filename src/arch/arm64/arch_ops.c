#include <stddef.h>

#include "dwunw/arch_ops.h"
#include "../arch_ops_internal.h"

#define ARM64_REG_FP 29u
#define ARM64_REG_LR 30u
#define ARM64_FRAME_RECORD_SIZE 16u

static int
arm64_valid_reg(size_t idx)
{
    return idx < DWUNW_REGSET_SLOTS;
}

static dwunw_status_t
arm64_normalize(struct dwunw_regset *regs)
{
    if (!regs) {
        return DWUNW_ERR_INVALID_ARG;
    }

    regs->arch = DWUNW_ARCH_ARM64;
    if (regs->version == 0) {
        regs->version = DWUNW_REGSET_VERSION;
    }

    return DWUNW_OK;
}

static dwunw_status_t
arm64_compute_cfa(const struct dwunw_regset *regs, uint64_t *cfa)
{
    if (!regs || !cfa) {
        return DWUNW_ERR_INVALID_ARG;
    }

    if (arm64_valid_reg(ARM64_REG_FP)) {
        uint64_t fp = regs->regs[ARM64_REG_FP];
        if (fp != 0) {
            *cfa = fp + ARM64_FRAME_RECORD_SIZE; /* fp/lr pair lives on stack */
            return DWUNW_OK;
        }
    }

    *cfa = regs->sp;
    return DWUNW_OK;
}

static dwunw_status_t
arm64_read_return_addr(const struct dwunw_regset *regs, uint64_t *ra)
{
    if (!regs || !ra) {
        return DWUNW_ERR_INVALID_ARG;
    }

    if (!arm64_valid_reg(ARM64_REG_LR)) {
        return DWUNW_ERR_INVALID_ARG;
    }

    *ra = regs->regs[ARM64_REG_LR];
    if (*ra == 0) {
        *ra = regs->pc;
    }

    return DWUNW_OK;
}

static dwunw_status_t
arm64_open_frame(const struct dwunw_regset *regs, struct dwunw_frame_window *win)
{
    dwunw_status_t st;

    if (!regs || !win) {
        return DWUNW_ERR_INVALID_ARG;
    }

    st = arm64_compute_cfa(regs, &win->cfa);
    if (st != DWUNW_OK) {
        return st;
    }

    return arm64_read_return_addr(regs, &win->ra);
}

const struct dwunw_arch_ops *
dwunw_arch_ops_arm64(void)
{
    static const struct dwunw_arch_ops ops = {
        .arch = DWUNW_ARCH_ARM64,
        .name = "arm64",
        .gpr_count = 31,
        .normalize = arm64_normalize,
        .compute_cfa = arm64_compute_cfa,
        .read_return_addr = arm64_read_return_addr,
        .open_frame = arm64_open_frame,
    };

    return &ops;
}
