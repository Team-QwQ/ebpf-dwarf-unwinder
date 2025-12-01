#ifndef DWUNW_ARCH_OPS_H
#define DWUNW_ARCH_OPS_H

#include <stdint.h>
#include <stddef.h>

#include "dwunw/status.h"

#define DWUNW_REGSET_VERSION 1
#define DWUNW_REGSET_SLOTS   32

enum dwunw_arch_id {
    DWUNW_ARCH_INVALID = 0,
    DWUNW_ARCH_X86_64 = 1,
    DWUNW_ARCH_ARM64  = 2,
    DWUNW_ARCH_MIPS32 = 3
};

struct dwunw_regset {
    uint16_t arch;        /* dwunw_arch_id */
    uint16_t version;     /* DWUNW_REGSET_VERSION */
    uint32_t flags;       /* future expansion */
    uint64_t sp;
    uint64_t pc;
    uint64_t regs[DWUNW_REGSET_SLOTS];
};

struct dwunw_frame_window {
    uint64_t cfa;
    uint64_t ra;
};

typedef dwunw_status_t (*dwunw_arch_normalize_fn)(struct dwunw_regset *);
typedef dwunw_status_t (*dwunw_arch_compute_cfa_fn)(const struct dwunw_regset *, uint64_t *);
typedef dwunw_status_t (*dwunw_arch_read_ra_fn)(const struct dwunw_regset *, uint64_t *);

typedef dwunw_status_t (*dwunw_arch_open_frame_fn)(const struct dwunw_regset *,
                                                   struct dwunw_frame_window *);

struct dwunw_arch_ops {
    enum dwunw_arch_id arch;
    const char *name;
    size_t gpr_count;
    dwunw_arch_normalize_fn normalize;
    dwunw_arch_compute_cfa_fn compute_cfa;
    dwunw_arch_read_ra_fn read_return_addr;
    dwunw_arch_open_frame_fn open_frame;
};

const struct dwunw_arch_ops *dwunw_arch_resolve(enum dwunw_arch_id arch);
const struct dwunw_arch_ops *dwunw_arch_from_regset(const struct dwunw_regset *regset);

dwunw_status_t dwunw_regset_prepare(struct dwunw_regset *regset,
                                    enum dwunw_arch_id arch_id);

#endif /* DWUNW_ARCH_OPS_H */
