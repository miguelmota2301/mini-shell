/* Injecao de falhas apenas no executavel de testes, via --wrap do linker. */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

ssize_t __real_read(int fd, void *buffer, size_t count);
ssize_t __real_write(int fd, const void *buffer, size_t count);
pid_t __real_wait(int *status);

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
