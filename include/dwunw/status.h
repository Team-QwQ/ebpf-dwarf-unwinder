#ifndef DWUNW_STATUS_H
#define DWUNW_STATUS_H

/*
 * Shared status codes across public APIs. Negative values indicate
 * failures so they can be propagated directly as errno-like errors.
 */
typedef enum {
    DWUNW_OK = 0,
    DWUNW_ERR_INVALID_ARG = -1,
    DWUNW_ERR_UNSUPPORTED_ARCH = -2,
    DWUNW_ERR_NOT_IMPLEMENTED = -3
} dwunw_status_t;

#endif /* DWUNW_STATUS_H */
