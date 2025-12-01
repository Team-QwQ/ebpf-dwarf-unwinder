#include "dwunw/dwunw_api.h"

/*
 * Placeholder initialization hook. Later stages will extend this
 * to configure toolchains, caches, and architecture handlers.
 */
dwunw_status_t
dwunw_init(struct dwunw_context *ctx)
{
    if (!ctx) {
        return DWUNW_ERR_INVALID_ARG;
    }

    ctx->reserved = 0;
    return DWUNW_OK;
}
