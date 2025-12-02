// SPDX-License-Identifier: MIT
#ifndef DWUNW_STACK_READER_H
#define DWUNW_STACK_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "dwunw/status.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dwunw_stack_reader {
    uint32_t reserved;
};

struct dwunw_stack_reader_session {
    pid_t pid;
    pid_t tid;
    int mem_fd;
    unsigned int backend;
    bool attached;
};

dwunw_status_t dwunw_stack_reader_init(struct dwunw_stack_reader *reader);
void dwunw_stack_reader_shutdown(struct dwunw_stack_reader *reader);

dwunw_status_t dwunw_stack_reader_attach(struct dwunw_stack_reader *reader,
                                         pid_t pid,
                                         pid_t tid,
                                         struct dwunw_stack_reader_session *session);

void dwunw_stack_reader_detach(struct dwunw_stack_reader_session *session);

dwunw_status_t dwunw_stack_reader_read(struct dwunw_stack_reader_session *session,
                                       uint64_t address,
                                       void *dst,
                                       size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DWUNW_STACK_READER_H */
