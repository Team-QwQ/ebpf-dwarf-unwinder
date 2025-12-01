#include "dwunw/arch_ops.h"
#include "../arch_ops_internal.h"

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
    (void)regs;
    (void)cfa;
    return DWUNW_ERR_NOT_IMPLEMENTED;
}

static dwunw_status_t
arm64_read_return_addr(const struct dwunw_regset *regs, uint64_t *ra)
{
    (void)regs;
    (void)ra;
    return DWUNW_ERR_NOT_IMPLEMENTED;
}

static dwunw_status_t
arm64_open_frame(const struct dwunw_regset *regs, struct dwunw_frame_window *win)
{
    (void)regs;
    (void)win;
    return DWUNW_ERR_NOT_IMPLEMENTED;
}

const struct dwunw_arch_ops *
dwunw_arch_ops_arm64(void)
{
    static const struct dwunw_arch_ops ops = {
        .arch = DWUNW_ARCH_ARM64,
        .name = "arm64",
        .gpr_count = 32,
        .normalize = arm64_normalize,
        .compute_cfa = arm64_compute_cfa,
        .read_return_addr = arm64_read_return_addr,
        .open_frame = arm64_open_frame,
    };

    return &ops;
}
