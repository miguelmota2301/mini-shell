#ifndef PARSER_H
#define PARSER_H

#define MAX_INPUT 1024
#define MAX_ARGS 64

//Mostra o "> " usando a chamada write()
void display_prompt(void);

//Lê a linha do terminal usando read()
//Retorna a quantidade de bytes lidos
int read_input(char *buffer);

//Quebra o buffer lido em um vetor de argumentos para o execvp()
//Retorna 1 se deve continuar, 0 se o comando for "exit". Caso seja 0, o loop principal deve encerrar.
int parse_input(char *buffer, char **args);

#endif