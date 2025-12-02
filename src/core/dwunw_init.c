#include <string.h>

#include "dwunw/dwunw_api.h"

dwunw_status_t
dwunw_init(struct dwunw_context *ctx)
{
    if (!ctx) {
        return DWUNW_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    dwunw_module_cache_init(&ctx->module_cache);
    ctx->module_cache_ready = 1;

    if (dwunw_stack_reader_init(&ctx->stack_reader) == DWUNW_OK) {
        ctx->stack_reader_ready = 1;
    }
    return DWUNW_OK;
}

void
dwunw_shutdown(struct dwunw_context *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->module_cache_ready) {
        dwunw_module_cache_flush(&ctx->module_cache);
        ctx->module_cache_ready = 0;
    }

    if (ctx->stack_reader_ready) {
        dwunw_stack_reader_shutdown(&ctx->stack_reader);
        ctx->stack_reader_ready = 0;
    }
}
