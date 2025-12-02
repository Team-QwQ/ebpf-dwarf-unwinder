#ifndef DWUNW_UNWIND_H
#define DWUNW_UNWIND_H

#include <stddef.h>
#include <stdint.h>

#include "dwunw/config.h"
#include "dwunw/arch_ops.h"
#include "dwunw/status.h"

struct dwunw_frame {
    uint64_t pc;
    uint64_t sp;
    uint64_t cfa;
    uint64_t ra;
    uint32_t flags;
    char module_path[DWUNW_MAX_PATH_LEN];
};

typedef dwunw_status_t (*dwunw_memory_read_fn)(void *ctx,
                                               uint64_t address,
                                               void *dst,
                                               size_t size);

struct dwunw_unwind_request {
    const char *module_path;
    const struct dwunw_regset *regs;
    struct dwunw_frame *frames;
    size_t max_frames;
    uint32_t options;
    dwunw_memory_read_fn read_memory;
    void *memory_ctx;
};

enum {
    DWUNW_OPTION_NONE = 0,
};

enum {
    DWUNW_FRAME_FLAG_PARTIAL = 1u << 0,
};

struct dwunw_context;

dwunw_status_t dwunw_capture(struct dwunw_context *ctx,
                             const struct dwunw_unwind_request *request,
                             size_t *frames_written);

#endif /* DWUNW_UNWIND_H */
