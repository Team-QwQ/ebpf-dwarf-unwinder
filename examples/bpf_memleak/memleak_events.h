#ifndef MEMLEAK_EVENTS_H
#define MEMLEAK_EVENTS_H

#ifdef __BPF__
#include <linux/types.h>
#define MEMLEAK_REGSET_SLOTS 32
#else
#include <stdint.h>
#include <string.h>

#include "dwunw/arch_ops.h"

#define MEMLEAK_REGSET_SLOTS DWUNW_REGSET_SLOTS
#endif

#define MEMLEAK_REGSET_VERSION 1
#define MEMLEAK_ARCH_INVALID 0
#define MEMLEAK_ARCH_X86_64 1
#define MEMLEAK_ARCH_ARM64  2
#define MEMLEAK_ARCH_MIPS32 3

struct memleak_event {
#ifdef __BPF__
    __u32 pid;
    __u32 tid;
    __u16 arch;
    __u16 reg_version;
    __u64 timestamp_ns;
    __u64 sp;
    __u64 pc;
    __u64 regs[MEMLEAK_REGSET_SLOTS];
    __u64 cookie;
    char comm[16];
#else
    uint32_t pid;
    uint32_t tid;
    uint16_t arch;
    uint16_t reg_version;
    uint64_t timestamp_ns;
    uint64_t sp;
    uint64_t pc;
    uint64_t regs[MEMLEAK_REGSET_SLOTS];
    uint64_t cookie;
    char comm[16];
#endif
};

#ifndef __BPF__
static inline dwunw_status_t
memleak_event_to_regset(const struct memleak_event *evt, struct dwunw_regset *out)
{
    dwunw_status_t st;
    size_t i, limit;

    if (!evt || !out) {
        return DWUNW_ERR_INVALID_ARG;
    }

    st = dwunw_regset_prepare(out, (enum dwunw_arch_id)evt->arch);
    if (st != DWUNW_OK) {
        return st;
    }

    out->sp = evt->sp;
    out->pc = evt->pc;

    limit = MEMLEAK_REGSET_SLOTS;
    if (limit > DWUNW_REGSET_SLOTS) {
        limit = DWUNW_REGSET_SLOTS;
    }

    for (i = 0; i < limit; ++i) {
        out->regs[i] = evt->regs[i];
    }

    return DWUNW_OK;
}
#endif

#endif /* MEMLEAK_EVENTS_H */
