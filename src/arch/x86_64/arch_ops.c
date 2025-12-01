#include <string.h>

#include "dwunw/arch_ops.h"
#include "../arch_ops_internal.h"

static dwunw_status_t
x86_64_normalize(struct dwunw_regset *regs)
{
    if (!regs) {
        return DWUNW_ERR_INVALID_ARG;
    }

    regs->arch = DWUNW_ARCH_X86_64;
    if (regs->version == 0) {
        regs->version = DWUNW_REGSET_VERSION;
    }
    return DWUNW_OK;
}

static dwunw_status_t
x86_64_compute_cfa(const struct dwunw_regset *regs, uint64_t *cfa)
{
    if (!regs || !cfa) {
        return DWUNW_ERR_INVALID_ARG;
    }

    *cfa = regs->sp;
    return DWUNW_OK;
}

static dwunw_status_t
x86_64_read_return_addr(const struct dwunw_regset *regs, uint64_t *ra)
{
    if (!regs || !ra) {
        return DWUNW_ERR_INVALID_ARG;
    }

    *ra = regs->pc;
    return DWUNW_OK;
}

static dwunw_status_t
x86_64_open_frame(const struct dwunw_regset *regs, struct dwunw_frame_window *win)
{
    if (!regs || !win) {
        return DWUNW_ERR_INVALID_ARG;
    }

    win->cfa = regs->sp;
    win->ra = regs->pc;
    return DWUNW_OK;
}

const struct dwunw_arch_ops *
dwunw_arch_ops_x86_64(void)
{
    static const struct dwunw_arch_ops ops = {
        .arch = DWUNW_ARCH_X86_64,
        .name = "x86_64",
        .gpr_count = 16,
        .normalize = x86_64_normalize,
        .compute_cfa = x86_64_compute_cfa,
        .read_return_addr = x86_64_read_return_addr,
        .open_frame = x86_64_open_frame,
    };

    return &ops;
}
