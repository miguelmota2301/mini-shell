#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
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

typedef enum {
    TOKEN_END, TOKEN_WORD, TOKEN_PIPE, TOKEN_INPUT,
    TOKEN_OUTPUT, TOKEN_APPEND, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *text;
} Token;

static Token next_token(const char **cursor, char **storage) {
    while (isspace((unsigned char)**cursor)) {
        (*cursor)++;
    }
    char current = **cursor;
    if (current == '\0') {
        return (Token){TOKEN_END, NULL};
    }
    if (current == '|' || current == '<' || current == '>') {
        (*cursor)++;
        if (current == '|') {
            return (Token){TOKEN_PIPE, NULL};
        }
        if (current == '<') {
            return (Token){TOKEN_INPUT, NULL};
        }
        if (**cursor == '>') {
            (*cursor)++;
            return (Token){TOKEN_APPEND, NULL};
        }
        return (Token){TOKEN_OUTPUT, NULL};
    }

    char *word = *storage;
    while (**cursor != '\0' && !isspace((unsigned char)**cursor)
           && strchr("|<>", **cursor) == NULL) {
        if (**cursor == '\'' || **cursor == '"') {
            char quote = *(*cursor)++;
            while (**cursor != '\0' && **cursor != quote) {
                *(*storage)++ = *(*cursor)++;
            }
            if (**cursor == '\0') {
                return (Token){TOKEN_ERROR, NULL};
            }
            (*cursor)++;
        } else {
            *(*storage)++ = *(*cursor)++;
        }
    }
    *(*storage)++ = '\0';
    return (Token){TOKEN_WORD, word};
}

static int syntax_error(const char *message) {
    fprintf(stderr, "Erro de sintaxe: %s\n", message);
    return -1;
}

int parse_input(const char *buffer, Pipeline *pipeline) {
    memset(pipeline, 0, sizeof(*pipeline));
    if (strlen(buffer) >= MAX_INPUT) {
        return syntax_error("entrada muito longa");
    }
    //As palavras e seus terminadores ocupam no maximo strlen(buffer) + 1.
    const char *cursor = buffer;
    char *storage = pipeline->storage;
    pipeline->count = 1;
    Command *command = &pipeline->commands[0];

    for (;;) {
        Token token = next_token(&cursor, &storage);
        if (token.type == TOKEN_ERROR) {
            return syntax_error("aspas não fechadas");
        }
        if (token.type == TOKEN_END) {
            break;
        }
        if (token.type == TOKEN_WORD) {
            if (command->argc >= MAX_ARGS - 1) {
                return syntax_error("limite de argumentos excedido");
            }
            command->args[command->argc++] = token.text;
        } else if (token.type == TOKEN_PIPE) {
            if (command->argc == 0) {
                return syntax_error("comando ausente antes de |");
            }
            if (pipeline->count >= MAX_COMMANDS) {
                return syntax_error("limite de comandos no pipeline excedido");
            }
            command = &pipeline->commands[pipeline->count++];
        } else {
            if (command->redirection_count >= MAX_REDIRECTIONS) {
                return syntax_error("limite de redirecionamentos excedido");
            }
            Token filename = next_token(&cursor, &storage);
            if (filename.type == TOKEN_ERROR) {
                return syntax_error("aspas não fechadas");
            }
            if (filename.type != TOKEN_WORD) {
                return syntax_error("arquivo ausente após redirecionamento");
            }
            RedirectionType type = token.type == TOKEN_INPUT ? REDIRECT_INPUT
                                  : token.type == TOKEN_APPEND ? REDIRECT_APPEND
                                  : REDIRECT_OUTPUT;
            command->redirections[command->redirection_count++] =
                (Redirection){type, filename.text};
        }
    }

    if (command->argc == 0) {
        if (pipeline->count == 1 && command->redirection_count == 0) {
            pipeline->count = 0;
            return 1;
        }
        return syntax_error("comando ausente");
    }
    //exit em um pipeline sera executado apenas no respectivo filho.
    if (pipeline->count == 1 && strcmp(command->args[0], "exit") == 0) {
        if (command->redirection_count != 0) {
            return syntax_error("exit isolado não aceita redirecionamentos");
        }
        return 0;
    }
    return 1;
}
