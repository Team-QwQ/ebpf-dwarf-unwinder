#ifndef DWUNW_API_H
#define DWUNW_API_H

#include <stddef.h>
#include <stdint.h>

#include "dwunw/config.h"
#include "dwunw/module_cache.h"
#include "dwunw/stack_reader.h"
#include "dwunw/status.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dwunw_context {
    uint32_t abi_tag;
    uint32_t reserved;
    struct dwunw_module_cache module_cache;
    uint8_t module_cache_ready;
    struct dwunw_stack_reader stack_reader;
    uint8_t stack_reader_ready;
};

static inline uint32_t
DWUNW_API_VERSION(void)
{
    return (DWUNW_VERSION_MAJOR << 16) |
           (DWUNW_VERSION_MINOR << 8) |
           DWUNW_VERSION_PATCH;
}

/* Library lifecycle ------------------------------------------------------- */
dwunw_status_t dwunw_init(struct dwunw_context *ctx);
void dwunw_shutdown(struct dwunw_context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DWUNW_API_H */
