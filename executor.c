#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "executor.h"

static void close_descriptor(int fd) {
    if (close(fd) < 0) {
        perror("Erro ao fechar descritor");
    }
}

static void close_pipes(int pipes[][2], int count) {
    for (int i = 0; i < count; i++) {
        close_descriptor(pipes[i][0]);
        close_descriptor(pipes[i][1]);
    }
}

static int redirect_descriptor(int source, int destination) {
    if (source == destination) {
        return 0;
    }
    int result;
    do {
        result = dup2(source, destination);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        perror("Erro ao redirecionar descritor");
        return -1;
    }
    return 0;
}

static int apply_redirections(const Command *command) {
    for (int i = 0; i < command->redirection_count; i++) {
        const Redirection *redirection = &command->redirections[i];
        int destination = redirection->type == REDIRECT_INPUT
                          ? STDIN_FILENO : STDOUT_FILENO;
        int flags = O_RDONLY;
        if (redirection->type != REDIRECT_INPUT) {
            flags = O_WRONLY | O_CREAT;
            flags |= redirection->type == REDIRECT_APPEND ? O_APPEND : O_TRUNC;
        }
        int fd;
        do {
            fd = open(redirection->path, flags, 0666);
        } while (fd < 0 && errno == EINTR);
        if (fd < 0) {
            fprintf(stderr, "Erro ao abrir '%s': %s\n",
                    redirection->path, strerror(errno));
            return -1;
        }
        int result = redirect_descriptor(fd, destination);
        if (fd != destination) {
            close_descriptor(fd);
        }
        if (result < 0) {
            return -1;
        }
    }
    return 0;
}

static void run_command(const Command *command) {
    if (apply_redirections(command) < 0) {
        _exit(EXIT_FAILURE);
    }
    if (strcmp(command->args[0], "exit") == 0) {
        _exit(EXIT_SUCCESS);
    }
    execvp(command->args[0], command->args);

    if (errno == EPERM) {
        fprintf(stderr, "Permissão negada ao executar: %s\n", command->args[0]);
    } else if (errno == ENOENT) {
        fprintf(stderr, "Arquivo ou diretório não existente: %s\n", command->args[0]);
    } else if (errno == EACCES) {
        fprintf(stderr, "Acesso negado: %s\n", command->args[0]);
    } else {
        perror(command->args[0]);
    }
    _exit(127);
}

static void wait_for_children(int count) {
    for (int i = 0; i < count; i++) {
        int status;
        pid_t waited_pid;
        do {
            waited_pid = wait(&status);
        } while (waited_pid < 0 && errno == EINTR);

        if (waited_pid < 0) {
            perror("Erro ao aguardar o processo filho");
            break;
        }
    }
}

void execute_pipeline(const Pipeline *pipeline) {
    int pipes[MAX_COMMANDS - 1][2];
    int pipe_count = 0;
    //Cria todos os pipes antes de iniciar os filhos.
    for (int i = 0; i < pipeline->count - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("Erro ao criar pipe");
            close_pipes(pipes, pipe_count);
            return;
        }
        pipe_count++;
    }

    pid_t children[MAX_COMMANDS];
    int started = 0;
    for (int i = 0; i < pipeline->count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Erro ao criar processo filho");
            break;
        }
        if (pid == 0) {
            if (i > 0 && redirect_descriptor(pipes[i - 1][0], STDIN_FILENO) < 0) {
                _exit(EXIT_FAILURE);
            }
            if (i < pipe_count && redirect_descriptor(pipes[i][1], STDOUT_FILENO) < 0) {
                _exit(EXIT_FAILURE);
            }
            //So os descritores 0 e 1 conectados ao comando ficam abertos.
            close_pipes(pipes, pipe_count);
            //Redirecionamentos de arquivos tem prioridade sobre o pipe.
            run_command(&pipeline->commands[i]);
        }
        children[started++] = pid;
    }

    close_pipes(pipes, pipe_count);
    if (started < pipeline->count) {
        //Cancela apenas os filhos deste pipeline incompleto antes de coleta-los.
        for (int i = 0; i < started; i++) {
            if (kill(children[i], SIGKILL) < 0 && errno != ESRCH) {
                perror("Erro ao encerrar processo filho");
            }
        }
    }
    //Todos os filhos foram iniciados antes de qualquer espera.
    wait_for_children(started);
}
