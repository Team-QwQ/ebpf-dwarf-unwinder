#include <stddef.h>

#include "dwunw/arch_ops.h"
#include "../arch/arch_ops_internal.h"

dwunw_status_t
dwunw_regset_prepare(struct dwunw_regset *regset, enum dwunw_arch_id arch_id)
{
    const struct dwunw_arch_ops *ops;

    if (!regset) {
        return DWUNW_ERR_INVALID_ARG;
    }

    ops = dwunw_arch_resolve(arch_id);
    if (!ops) {
        return DWUNW_ERR_UNSUPPORTED_ARCH;
    }

    regset->arch = arch_id;
    regset->version = DWUNW_REGSET_VERSION;
    regset->flags = 0;
    regset->sp = 0;
    regset->pc = 0;
    for (size_t i = 0; i < DWUNW_REGSET_SLOTS; ++i) {
        regset->regs[i] = 0;
    }

    return ops->normalize(regset);
}

const struct dwunw_arch_ops *
dwunw_arch_resolve(enum dwunw_arch_id arch)
{
    switch (arch) {
    case DWUNW_ARCH_X86_64:
        return dwunw_arch_ops_x86_64();
    case DWUNW_ARCH_ARM64:
        return dwunw_arch_ops_arm64();
    case DWUNW_ARCH_MIPS32:
        return dwunw_arch_ops_mips32();
    default:
        return NULL;
    }
}

const struct dwunw_arch_ops *
dwunw_arch_from_regset(const struct dwunw_regset *regset)
{
    if (!regset) {
        return NULL;
    }

    return dwunw_arch_resolve((enum dwunw_arch_id)regset->arch);
}
