#ifndef DWUNW_API_H
#define DWUNW_API_H

#include <stddef.h>
#include <stdint.h>

#include "dwunw/config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DWUNW_OK = 0,
    DWUNW_ERR_INVALID_ARG = -1,
    DWUNW_ERR_UNSUPPORTED_ARCH = -2,
    DWUNW_ERR_NOT_IMPLEMENTED = -3
} dwunw_status_t;

struct dwunw_context {
    uint32_t abi_tag;
    uint32_t reserved;
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

#ifdef __cplusplus
}
#endif

#endif /* DWUNW_API_H */
