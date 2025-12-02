// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dwunw/stack_reader.h"

#define DWUNW_STACK_READER_BACKEND_NONE 0u
#define DWUNW_STACK_READER_BACKEND_PROCESS_VM 1u
#define DWUNW_STACK_READER_BACKEND_PROC_MEM 2u

static dwunw_status_t
stack_reader_process_vm_read(struct dwunw_stack_reader_session *session,
                             uint64_t address,
                             void *dst,
                             size_t size)
{
    struct iovec local_iov = {
        .iov_base = dst,
        .iov_len = size,
    };
    struct iovec remote_iov = {
        .iov_base = (void *)(uintptr_t)address,
        .iov_len = size,
    };
    ssize_t copied = process_vm_readv(session->tid, &local_iov, 1, &remote_iov, 1, 0);
    if (copied == (ssize_t)size) {
        return DWUNW_OK;
    }

    if (copied >= 0) {
        return DWUNW_ERR_IO;
    }

    switch (errno) {
    case ENOSYS:
    case EPERM:
    case ESRCH:
    case EFAULT:
        return DWUNW_ERR_NOT_IMPLEMENTED;
    default:
        return DWUNW_ERR_IO;
    }
}

static dwunw_status_t
stack_reader_proc_mem_read(struct dwunw_stack_reader_session *session,
                           uint64_t address,
                           void *dst,
                           size_t size)
{
    if (session->mem_fd < 0) {
        char path[64];
        int len = snprintf(path, sizeof(path), "/proc/%d/mem", session->pid);
        if (len <= 0 || (size_t)len >= sizeof(path)) {
            return DWUNW_ERR_INVALID_ARG;
        }

        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return DWUNW_ERR_IO;
        }
        session->mem_fd = fd;
    }

    ssize_t copied = pread(session->mem_fd, dst, size, (off_t)address);
    if (copied == (ssize_t)size) {
        return DWUNW_OK;
    }

    return DWUNW_ERR_IO;
}

dwunw_status_t
dwunw_stack_reader_init(struct dwunw_stack_reader *reader)
{
    if (!reader) {
        return DWUNW_ERR_INVALID_ARG;
    }

    reader->reserved = 0;
    return DWUNW_OK;
}

void
dwunw_stack_reader_shutdown(struct dwunw_stack_reader *reader)
{
    (void)reader;
}

dwunw_status_t
dwunw_stack_reader_attach(struct dwunw_stack_reader *reader,
                          pid_t pid,
                          pid_t tid,
                          struct dwunw_stack_reader_session *session)
{
    (void)reader;
    if (!session || pid <= 0) {
        return DWUNW_ERR_INVALID_ARG;
    }

    if (tid <= 0) {
        tid = pid;
    }

    memset(session, 0, sizeof(*session));
    session->pid = pid;
    session->tid = tid;
    session->mem_fd = -1;

    if (ptrace(PTRACE_ATTACH, tid, NULL, NULL) == -1) {
        memset(session, 0, sizeof(*session));
        return DWUNW_ERR_IO;
    }

    int status = 0;
    int wait_ret;
    do {
        wait_ret = waitpid(tid, &status, 0);
    } while (wait_ret == -1 && errno == EINTR);

    if (wait_ret == -1) {
        ptrace(PTRACE_DETACH, tid, NULL, NULL);
        memset(session, 0, sizeof(*session));
        return DWUNW_ERR_IO;
    }

    if (!WIFSTOPPED(status)) {
        ptrace(PTRACE_DETACH, tid, NULL, NULL);
        memset(session, 0, sizeof(*session));
        return DWUNW_ERR_IO;
    }

    session->attached = true;
    session->backend = DWUNW_STACK_READER_BACKEND_PROCESS_VM;
    return DWUNW_OK;
}

void
dwunw_stack_reader_detach(struct dwunw_stack_reader_session *session)
{
    if (!session) {
        return;
    }

    if (session->mem_fd >= 0) {
        close(session->mem_fd);
        session->mem_fd = -1;
    }

    if (session->attached) {
        ptrace(PTRACE_DETACH, session->tid, NULL, NULL);
        session->attached = false;
    }

    memset(session, 0, sizeof(*session));
}

dwunw_status_t
dwunw_stack_reader_read(struct dwunw_stack_reader_session *session,
                        uint64_t address,
                        void *dst,
                        size_t size)
{
    if (!session || !dst || size == 0) {
        return DWUNW_ERR_INVALID_ARG;
    }

    dwunw_status_t status;

    if (session->backend == DWUNW_STACK_READER_BACKEND_PROCESS_VM) {
        status = stack_reader_process_vm_read(session, address, dst, size);
        if (status == DWUNW_OK) {
            return DWUNW_OK;
        }

        if (status != DWUNW_ERR_NOT_IMPLEMENTED) {
            return status;
        }

        session->backend = DWUNW_STACK_READER_BACKEND_PROC_MEM;
    }

    if (session->backend == DWUNW_STACK_READER_BACKEND_PROC_MEM) {
        return stack_reader_proc_mem_read(session, address, dst, size);
    }

    return DWUNW_ERR_NOT_IMPLEMENTED;
}
