#include <string.h>

#include "dwunw/dwunw_api.h"
#include "dwunw/module_cache.h"
#include "dwunw/unwind.h"

static dwunw_status_t
prepare_root_frame(const struct dwunw_regset *regs, struct dwunw_frame *frame)
{
    const struct dwunw_arch_ops *ops;
    dwunw_status_t status;
    struct dwunw_frame_window win;
    uint64_t cfa = 0;

    if (!regs || !frame) {
        return DWUNW_ERR_INVALID_ARG;
    }

    ops = dwunw_arch_from_regset(regs);
    if (!ops) {
        return DWUNW_ERR_UNSUPPORTED_ARCH;
    }

    status = ops->compute_cfa ? ops->compute_cfa(regs, &cfa) : DWUNW_ERR_NOT_IMPLEMENTED;
    if (status == DWUNW_ERR_NOT_IMPLEMENTED) {
        cfa = regs->sp;
    } else if (status != DWUNW_OK) {
        return status;
    }

    status = ops->open_frame ? ops->open_frame(regs, &win) : DWUNW_ERR_NOT_IMPLEMENTED;
    if (status == DWUNW_ERR_NOT_IMPLEMENTED) {
        win.cfa = cfa;
        status = ops->read_return_addr ? ops->read_return_addr(regs, &win.ra) : DWUNW_ERR_NOT_IMPLEMENTED;
        if (status == DWUNW_ERR_NOT_IMPLEMENTED) {
            win.ra = regs->pc;
        } else if (status != DWUNW_OK) {
            return status;
        }
    } else if (status != DWUNW_OK) {
        return status;
    }

    frame->pc = regs->pc;
    frame->sp = regs->sp;
    frame->cfa = win.cfa;
    frame->ra = win.ra;
    frame->flags = DWUNW_FRAME_FLAG_PARTIAL;
    return DWUNW_OK;
}

dwunw_status_t
dwunw_capture(struct dwunw_context *ctx,
             const struct dwunw_unwind_request *request,
             size_t *frames_written)
{
    struct dwunw_module_handle *handle = NULL;
    dwunw_status_t status;
    size_t produced = 0;

    if (frames_written) {
        *frames_written = 0;
    }

    if (!ctx || !request || !request->regs || !request->frames ||
        request->max_frames == 0 || !request->module_path) {
        return DWUNW_ERR_INVALID_ARG;
    }

    if (!ctx->module_cache_ready) {
        return DWUNW_ERR_INVALID_ARG;
    }

    status = dwunw_module_cache_acquire(&ctx->module_cache,
                                        request->module_path,
                                        &handle);
    if (status != DWUNW_OK) {
        return status;
    }

    status = prepare_root_frame(request->regs, &request->frames[0]);
    if (status == DWUNW_OK) {
        strncpy(request->frames[0].module_path,
                request->module_path,
                sizeof(request->frames[0].module_path) - 1);
        request->frames[0].module_path[sizeof(request->frames[0].module_path) - 1] = '\0';
        produced = 1;
    }

    dwunw_module_cache_release(&ctx->module_cache, handle);

    if (frames_written) {
        *frames_written = produced;
    }

    return status;
}
