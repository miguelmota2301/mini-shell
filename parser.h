#ifndef PARSER_H
#define PARSER_H

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_COMMANDS 16
#define MAX_REDIRECTIONS 16

typedef enum {
    REDIRECT_INPUT,
    REDIRECT_OUTPUT,
    REDIRECT_APPEND
} RedirectionType;

typedef struct {
    RedirectionType type;
    char *path;
} Redirection;

typedef struct {
    char *args[MAX_ARGS];
    int argc;
    Redirection redirections[MAX_REDIRECTIONS];
    int redirection_count;
} Command;

typedef struct {
    Command commands[MAX_COMMANDS];
    int count;
    //Armazena as palavras, sem os operadores e as aspas.
    char storage[MAX_INPUT];
} Pipeline;

//Mostra o "> " usando a chamada write()
//Retorna 0 em caso de sucesso ou -1 em caso de erro.
int display_prompt(void);

//Lê a linha do terminal usando read()
//Repete a leitura se interrompida por sinal (EINTR).
//Retorna os bytes lidos, 0 para EOF ou -1 em caso de erro.
int read_input(char *buffer);

//Separa comandos, argumentos, pipes e redirecionamentos.
//Retorna 1 para continuar, 0 para exit isolado ou -1 para erro de sintaxe.
int parse_input(const char *buffer, Pipeline *pipeline);

#endif
