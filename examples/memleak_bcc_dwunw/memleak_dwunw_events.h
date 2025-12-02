// SPDX-License-Identifier: MIT
#ifndef MEMLEAK_DWUNW_EVENTS_H
#define MEMLEAK_DWUNW_EVENTS_H

#ifdef __BPF__
#include <vmlinux.h>
#else
#include <linux/types.h>
#endif

#define MEMLEAK_DWUNW_REGSET_VERSION 1
#define MEMLEAK_DWUNW_REGSET_SLOTS 32
#define MEMLEAK_DWUNW_ARCH_X86_64 1

struct memleak_dwunw_regset_snapshot {
    __u16 arch;
    __u16 version;
    __u32 flags;
    __u64 sp;
    __u64 pc;
    __u64 regs[MEMLEAK_DWUNW_REGSET_SLOTS];
};

struct memleak_dwunw_event {
    __u32 pid;
    __u32 tgid;
    __u32 arch;
    __u32 reserved;
    char comm[16];
    struct memleak_dwunw_regset_snapshot regset;
};

#endif /* MEMLEAK_DWUNW_EVENTS_H */
