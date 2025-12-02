#include <stdbool.h>
#include <string.h>

#include "dwunw/dwunw_api.h"
#include "dwunw/module_cache.h"
#include "dwunw/stack_reader.h"
#include "dwunw/unwind.h"
#include "dwarf/cfi.h"

static dwunw_status_t
default_stack_reader_mem(void *ctx, uint64_t address, void *dst, size_t size)
{
    struct dwunw_stack_reader_session *session = ctx;
    return dwunw_stack_reader_read(session, address, dst, size);
}

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
    struct dwunw_unwind_request effective = {0};
    struct dwunw_module_handle *handle = NULL;
    struct dwunw_stack_reader_session session;
    bool using_stack_reader = false;
    dwunw_status_t reader_status = DWUNW_OK;
    dwunw_status_t status = DWUNW_OK;
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

    effective = *request;

    if (!effective.read_memory && ctx->stack_reader_ready &&
        effective.pid > 0 && effective.max_frames > 1) {
        dwunw_status_t attach_status = dwunw_stack_reader_attach(&ctx->stack_reader,
                                                                 effective.pid,
                                                                 effective.tid,
                                                                 &session);
        if (attach_status == DWUNW_OK) {
            effective.read_memory = default_stack_reader_mem;
            effective.memory_ctx = &session;
            using_stack_reader = true;
        } else {
            reader_status = attach_status;
        }
    }

    status = dwunw_module_cache_acquire(&ctx->module_cache,
                                        effective.module_path,
                                        &handle);
    if (status != DWUNW_OK) {
        if (using_stack_reader) {
            dwunw_stack_reader_detach(&session);
        }
        return status;
    }

    status = prepare_root_frame(effective.regs, &effective.frames[0]);
    if (status == DWUNW_OK) {
        strncpy(effective.frames[0].module_path,
                effective.module_path,
                sizeof(effective.frames[0].module_path) - 1);
        effective.frames[0].module_path[sizeof(effective.frames[0].module_path) - 1] = '\0';
        produced = 1;

        if (effective.max_frames > 1 && handle->index.fde_count > 0 &&
            effective.read_memory) {
            struct dwunw_regset cursor_regs = *effective.regs;
            const struct dwunw_arch_ops *ops = dwunw_arch_from_regset(&cursor_regs);

            while (produced < effective.max_frames) {
                const struct dwunw_fde_record *fde;
                struct dwunw_frame *cursor_frame;
                dwunw_status_t unwind_status;

                fde = dwunw_cfi_find_fde(handle->index.fdes,
                                         handle->index.fde_count,
                                         cursor_regs.pc);
                if (!fde) {
                    break;
                }

                cursor_frame = &effective.frames[produced];
                unwind_status = dwunw_cfi_eval(fde,
                                               cursor_regs.pc,
                                               &cursor_regs,
                                               effective.read_memory,
                                               effective.memory_ctx,
                                               cursor_frame);
                if (unwind_status != DWUNW_OK) {
                    status = unwind_status;
                    break;
                }

                cursor_frame->flags &= ~DWUNW_FRAME_FLAG_PARTIAL;
                strncpy(cursor_frame->module_path,
                        effective.module_path,
                        sizeof(cursor_frame->module_path) - 1);
                cursor_frame->module_path[sizeof(cursor_frame->module_path) - 1] = '\0';
                produced++;

                if (ops && ops->normalize) {
                    ops->normalize(&cursor_regs);
                }
            }
        }
    }

    dwunw_module_cache_release(&ctx->module_cache, handle);

    if (using_stack_reader) {
        dwunw_stack_reader_detach(&session);
    }

    if (status == DWUNW_OK && reader_status != DWUNW_OK) {
        status = reader_status;
    }

    if (frames_written) {
        *frames_written = produced;
    }

    return status;
}
