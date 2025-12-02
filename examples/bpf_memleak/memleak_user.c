// SPDX-License-Identifier: MIT
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <bpf/libbpf.h>
#pragma GCC diagnostic pop

#include "dwunw/dwunw_api.h"
#include "dwunw/unwind.h"
#include "memleak_events.h"

static volatile sig_atomic_t exiting;

struct app_config {
    const char *bpf_obj_path;
    const char *symbol;
    int duration_sec;
    int verbose;
};

struct proc_mem_reader {
    int fd;
    uint32_t pid;
};

static void
proc_mem_reader_close(struct proc_mem_reader *reader)
{
    if (reader && reader->fd >= 0) {
        close(reader->fd);
        reader->fd = -1;
    }
}

static int
proc_mem_reader_open(struct proc_mem_reader *reader, uint32_t pid)
{
    char path[64];

    if (!reader)
        return -EINVAL;

    snprintf(path, sizeof(path), "/proc/%u/mem", pid);
    reader->pid = pid;
    reader->fd = open(path, O_RDONLY | O_CLOEXEC);
    if (reader->fd < 0)
        return -errno;

    return 0;
}

static dwunw_status_t
proc_mem_reader_read(void *ctx, uint64_t address, void *dst, size_t size)
{
    struct proc_mem_reader *reader = ctx;
    ssize_t n;

    if (!reader || reader->fd < 0)
        return DWUNW_ERR_INVALID_ARG;

    n = pread(reader->fd, dst, size, (off_t)address);
    if (n < 0)
        return errno == EFAULT ? DWUNW_ERR_INVALID_ARG : DWUNW_ERR_IO;
    if ((size_t)n != size)
        return DWUNW_ERR_IO;

    return DWUNW_OK;
}

static void
handle_signal(int sig)
{
    (void)sig;
    exiting = 1;
}

static void
setup_signal_handlers(void)
{
    struct sigaction sa = {
        .sa_handler = handle_signal,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void
usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [--bpf path] [--symbol sym] [--duration sec] [--quiet]\n",
            prog);
}

static int
parse_args(int argc, char **argv, struct app_config *cfg)
{
    static const struct option opts[] = {
        {"bpf", required_argument, NULL, 'b'},
        {"symbol", required_argument, NULL, 's'},
        {"duration", required_argument, NULL, 'd'},
        {"quiet", no_argument, NULL, 'q'},
        {"help", no_argument, NULL, 'h'},
        {}
    };
    int c;

    while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (c) {
        case 'b':
            cfg->bpf_obj_path = optarg;
            break;
        case 's':
            cfg->symbol = optarg;
            break;
        case 'd':
            cfg->duration_sec = atoi(optarg);
            break;
        case 'q':
            cfg->verbose = 0;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return -EINVAL;
        }
    }

    if (!cfg->bpf_obj_path || !cfg->symbol) {
        usage(argv[0]);
        return -EINVAL;
    }

    return 0;
}

static int
resolve_proc_exe(uint32_t pid, char *buf, size_t len)
{
    char path[64];
    ssize_t n;

    snprintf(path, sizeof(path), "/proc/%u/exe", pid);
    n = readlink(path, buf, len - 1);
    if (n < 0) {
        snprintf(buf, len, "[pid:%u]", pid);
        return -errno;
    }

    buf[n] = '\0';
    return 0;
}

static int
handle_event(void *ctx, void *data, size_t data_sz)
{
    struct dwunw_context *unw_ctx = ctx;
    const struct memleak_event *evt = data;
    struct dwunw_regset regs;
    struct dwunw_frame frames[8];
    struct dwunw_unwind_request req;
    struct proc_mem_reader reader = {
        .fd = -1,
        .pid = evt->pid,
    };
    int reader_status;
    int reader_enabled = 0;
    dwunw_status_t st;
    size_t written = 0;
    char module_path[DWUNW_MAX_PATH_LEN];

    (void)data_sz;

    st = memleak_event_to_regset(evt, &regs);
    if (st != DWUNW_OK) {
        fprintf(stderr, "[warn] memleak_event_to_regset error=%d\n", st);
        return 0;
    }

    if (resolve_proc_exe(evt->pid, module_path, sizeof(module_path)) < 0) {
        snprintf(module_path, sizeof(module_path), "[pid:%u]", evt->pid);
    }

    memset(frames, 0, sizeof(frames));
    memset(&req, 0, sizeof(req));
    req.module_path = module_path;
    req.regs = &regs;
    req.frames = frames;
    req.max_frames = 8;
    req.options = DWUNW_OPTION_NONE;

    reader_status = proc_mem_reader_open(&reader, evt->pid);
    if (reader_status == 0) {
        req.read_memory = proc_mem_reader_read;
        req.memory_ctx = &reader;
        reader_enabled = 1;
    } else {
        fprintf(stderr,
                "[warn] open /proc/%u/mem failed: %s (falling back to frame[0])\n",
                evt->pid,
                strerror(-reader_status));
    }

    st = dwunw_capture(unw_ctx, &req, &written);
    if (st != DWUNW_OK && reader_enabled) {
        fprintf(stderr,
                "[warn] multi-frame capture failed err=%d pid=%u comm=%s, retrying without reader\n",
                st,
                evt->pid,
                evt->comm);
        proc_mem_reader_close(&reader);
        reader_enabled = 0;
        req.read_memory = NULL;
        req.memory_ctx = NULL;
        memset(frames, 0, sizeof(frames));
        written = 0;
        st = dwunw_capture(unw_ctx, &req, &written);
    }

    proc_mem_reader_close(&reader);

    if (st != DWUNW_OK) {
        fprintf(stderr,
                "[warn] dwunw_capture error=%d pid=%u comm=%s\n",
                st,
                evt->pid,
                evt->comm);
        return 0;
    }

    printf("[event] pid=%u comm=%s pc=0x%llx\n",
           evt->pid,
           evt->comm,
           (unsigned long long)evt->pc);

    for (size_t i = 0; i < written; ++i) {
        const struct dwunw_frame *f = &frames[i];
        printf("  #%zu pc=0x%llx sp=0x%llx ra=0x%llx flags=0x%x\n",
               i,
               (unsigned long long)f->pc,
               (unsigned long long)f->sp,
               (unsigned long long)f->ra,
               f->flags);
    }

    return 0;
}

int
main(int argc, char **argv)
{
    struct dwunw_context ctx;
    struct app_config cfg = {
        .bpf_obj_path = NULL,
        .symbol = NULL,
        .duration_sec = 10,
        .verbose = 1,
    };
    struct bpf_object *obj = NULL;
    struct bpf_program *prog = NULL;
    struct bpf_link *link = NULL;
    struct bpf_map *events_map = NULL;
    struct ring_buffer *rb = NULL;
    int err;
    time_t deadline = 0;

    if (parse_args(argc, argv, &cfg) < 0) {
        return 1;
    }

    if (dwunw_init(&ctx) != DWUNW_OK) {
        fprintf(stderr, "failed to init dwunw context\n");
        return 1;
    }

    setup_signal_handlers();

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
    obj = bpf_object__open_file(cfg.bpf_obj_path, NULL);
    if (!obj) {
        fprintf(stderr, "failed to open %s: %s\n", cfg.bpf_obj_path, strerror(errno));
        goto out;
    }

    err = bpf_object__load(obj);
    if (err) {
        fprintf(stderr, "failed to load BPF object: %d\n", err);
        goto out;
    }

    prog = bpf_object__find_program_by_name(obj, "memleak_do_exit");
    if (!prog) {
        fprintf(stderr, "missing memleak_do_exit program\n");
        goto out;
    }

    link = bpf_program__attach_kprobe(prog, false, cfg.symbol);
    if (!link) {
        fprintf(stderr, "bpf_program__attach_kprobe failed: %s\n", strerror(errno));
        goto out;
    }

    events_map = bpf_object__find_map_by_name(obj, "events");
    if (!events_map) {
        fprintf(stderr, "missing events map\n");
        goto out;
    }

    rb = ring_buffer__new(bpf_map__fd(events_map), handle_event, &ctx, NULL);
    if (!rb) {
        fprintf(stderr, "failed to create ring buffer\n");
        goto out;
    }

    deadline = time(NULL) + cfg.duration_sec;
    printf("memleak_user: capturing via %s (duration=%ds)\n",
           cfg.symbol,
           cfg.duration_sec);

    while (!exiting && time(NULL) < deadline) {
        err = ring_buffer__poll(rb, 100);
        if (err > 0 || err == -EINTR) {
            continue;
        }
        if (err < 0) {
            fprintf(stderr, "ring_buffer__poll err=%d\n", err);
            break;
        }
    }

out:
    ring_buffer__free(rb);
    if (link) {
        bpf_link__destroy(link);
    }
    if (obj) {
        bpf_object__close(obj);
    }
    dwunw_shutdown(&ctx);
    return 0;
}
