// SPDX-License-Identifier: BSD-2-Clause
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>

#include "memleak_events.h"

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 18);
} events SEC(".maps");

static __always_inline void
memleak_capture_regs(struct memleak_event *evt, struct pt_regs *regs)
{
#if defined(__TARGET_ARCH_x86)
    evt->regs[0] = PT_REGS_AX(regs);
    evt->regs[1] = PT_REGS_BX(regs);
    evt->regs[2] = PT_REGS_CX(regs);
    evt->regs[3] = PT_REGS_DX(regs);
    evt->regs[4] = PT_REGS_SI(regs);
    evt->regs[5] = PT_REGS_DI(regs);
    evt->regs[6] = PT_REGS_BP(regs);
    evt->regs[7] = PT_REGS_SP(regs);
#else
    /* 其他架构占位，后续阶段补充 */
#endif
}

SEC("kprobe/do_exit")
int BPF_KPROBE(memleak_do_exit, struct pt_regs *regs)
{
    struct memleak_event *evt;
    __u64 pid_tgid;

    evt = bpf_ringbuf_reserve(&events, sizeof(*evt), 0);
    if (!evt) {
        return 0;
    }

    __builtin_memset(evt, 0, sizeof(*evt));

    pid_tgid = bpf_get_current_pid_tgid();
    evt->pid = pid_tgid >> 32;
    evt->tid = (__u32)pid_tgid;
    evt->timestamp_ns = bpf_ktime_get_ns();
    evt->arch = MEMLEAK_ARCH_X86_64;
    evt->reg_version = MEMLEAK_REGSET_VERSION;
    evt->sp = PT_REGS_SP(regs);
    evt->pc = PT_REGS_IP(regs);
    evt->cookie = (__u64)bpf_get_current_task();
    bpf_get_current_comm(evt->comm, sizeof(evt->comm));

    memleak_capture_regs(evt, regs);

    bpf_ringbuf_submit(evt, 0);
    return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
