/* Injecao de falhas apenas no executavel de testes, via --wrap do linker. */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

ssize_t __real_read(int fd, void *buffer, size_t count);
ssize_t __real_write(int fd, const void *buffer, size_t count);
pid_t __real_wait(int *status);
int __real_pipe(int descriptors[2]);
int __real_dup2(int source, int destination);
pid_t __real_fork(void);
int __real_open(const char *path, int flags, ...);

static int has_fault(const char *name) {
    const char *fault = getenv("MINISHELL_TEST_FAULT");
    return fault != NULL && strcmp(fault, name) == 0;
}

ssize_t __wrap_read(int fd, void *buffer, size_t count) {
    static int interruptions = 0;
    if (has_fault("read_eintr") && interruptions++ < 2) {
        errno = EINTR;
        return -1;
    }
    return __real_read(fd, buffer, count);
}

ssize_t __wrap_write(int fd, const void *buffer, size_t count) {
    static int interruptions = 0;
    if (has_fault("write_eintr") && interruptions++ < 2) {
        errno = EINTR;
        return -1;
    }
    if (has_fault("write_zero")) {
        return 0;
    }
    if (has_fault("write_partial") && count > 1) {
        count = 1;
    }
    return __real_write(fd, buffer, count);
}

pid_t __wrap_wait(int *status) {
    static int interruptions = 0;
    if (has_fault("wait_eintr") && interruptions++ < 2) {
        errno = EINTR;
        return -1;
    }
    pid_t result = __real_wait(status);
    if (has_fault("wait_error") && result > 0) {
        /* Coleta o filho antes da falha simulada para nao deixar zumbis. */
        errno = ECHILD;
        return -1;
    }
    return result;
}

int __wrap_pipe(int descriptors[2]) {
    static int calls = 0;
    int call = calls++;
    if (has_fault("pipe_error") || (has_fault("pipe_after_one") && call == 1)) {
        errno = EMFILE;
        return -1;
    }
    return __real_pipe(descriptors);
}

int __wrap_dup2(int source, int destination) {
    static int interruptions = 0;
    if (has_fault("dup2_eintr") && interruptions++ < 2) {
        errno = EINTR;
        return -1;
    }
    if (has_fault("dup2_error")) {
        errno = EBADF;
        return -1;
    }
    return __real_dup2(source, destination);
}

pid_t __wrap_fork(void) {
    static int calls = 0;
    int call = calls++;
    if (has_fault("fork_error") || (has_fault("fork_after_one") && call == 1)) {
        errno = EAGAIN;
        return -1;
    }
    return __real_fork();
}

int __wrap_open(const char *path, int flags, ...) {
    static int interruptions = 0;
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, int);
        va_end(arguments);
    }
    if (has_fault("open_eintr") && interruptions++ < 2) {
        errno = EINTR;
        return -1;
    }
    return __real_open(path, flags, mode);
}
