#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "executor.h"

void execute_command(char **args) {
    pid_t pid = fork();

    if (pid < 0){
        perror("Unable to fork");
        return;
    }
    
    if (pid == 0){
        execvp(args[0], args);

fprintf(stderr, "Comando: %s\n", args[0]);
fprintf(stderr, "Código errno: %d\n", errno);
perror("Detalhes do erro");

_exit(127);

        if (errno == EPERM){
            fprintf(stderr, "Permissão negada ao executar: %s\n", args[0]);
        } else if (errno == ENOENT){
            fprintf(stderr, "Arquivo ou diretório não existente: %s\n", args[0]);
        } else if (errno == EACCES){
            fprintf(stderr, "Acesso negado: %s\n", args[0]);
        } else {
            perror(args[0]);
        }

        _exit(127);

    } else {
        int status;
        wait(&status);
    }

    }