#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parser.h"


void display_prompt(void) {
    //Usa write() para exibir o prompt
    write(STDOUT_FILENO, "> ", 2);
}

int read_input(char *buffer) {
    //Lê até MAX_INPUT bytes diretamente da entrada, definido em parser.h
    ssize_t bytes_read = read(STDIN_FILENO, buffer, MAX_INPUT - 1);
    
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