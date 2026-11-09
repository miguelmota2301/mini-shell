#ifndef PARSER_H
#define PARSER_H

#define MAX_INPUT 1024
#define MAX_ARGS 64

//Mostra o "> " usando a chamada write()
//Retorna 0 em caso de sucesso ou -1 em caso de erro.
int display_prompt(void);

//Lê a linha do terminal usando read()
//Repete a leitura se interrompida por sinal (EINTR).
//Retorna os bytes lidos, 0 para EOF ou -1 em caso de erro.
int read_input(char *buffer);

//Quebra o buffer lido em um vetor de argumentos para o execvp()
//Retorna 1 se deve continuar, 0 se o comando for "exit". Caso seja 0, o loop principal deve encerrar.
int parse_input(char *buffer, char **args);

#endif
