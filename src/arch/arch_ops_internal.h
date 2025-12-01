#ifndef DWUNW_ARCH_OPS_INTERNAL_H
#define DWUNW_ARCH_OPS_INTERNAL_H

#include "dwunw/arch_ops.h"

const struct dwunw_arch_ops *dwunw_arch_ops_x86_64(void);
const struct dwunw_arch_ops *dwunw_arch_ops_arm64(void);
const struct dwunw_arch_ops *dwunw_arch_ops_mips32(void);

#endif /* DWUNW_ARCH_OPS_INTERNAL_H */
