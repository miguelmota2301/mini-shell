#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "parser.h"


int display_prompt(void) {
    const char prompt[] = "> ";
    size_t total_written = 0;

    //write() pode escrever menos bytes que o solicitado.
    while (total_written < sizeof(prompt) - 1) {
        ssize_t bytes_written = write(STDOUT_FILENO, prompt + total_written,
                                      sizeof(prompt) - 1 - total_written);
        if (bytes_written < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Erro ao exibir o prompt");
            return -1;
        }
        if (bytes_written == 0) {
            fprintf(stderr, "Erro ao exibir o prompt: nenhum byte foi escrito\n");
            return -1;
        }
        total_written += (size_t)bytes_written;
    }

    return 0;
}

int read_input(char *buffer) {
    //Reserva um byte do buffer para o terminador da string.
    ssize_t bytes_read;
    do {
        bytes_read = read(STDIN_FILENO, buffer, MAX_INPUT - 1);
    } while (bytes_read < 0 && errno == EINTR);

    if (bytes_read < 0) {
        perror("Erro ao ler a entrada");
    }
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else {
        buffer[0] = '\0';
    }
    
    return bytes_read;
}

int parse_input(char *buffer, char **args) {
    //Remove o "/n", se houver
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    //Separa os tokens por espaço
    int i = 0;
    char *token = strtok(buffer, " ");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL; //Importante: execvp exige NULL no final do vetor

    if (args[0] == NULL) {
        return 1;
    }

    //Verifica se o comando é "exit" para encerrar o loop
    if (strcmp(args[0], "exit") == 0) {
        return 0;
    }

    return 1;
}
