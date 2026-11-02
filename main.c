#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "parser.h"


void execute_command(char **args) {
    /*printf("Teste pessoa 1\n");
    printf("Comando principal: %s\n", args[0]);
    
    for (int i = 0; args[i] != NULL; i++) {
        printf("  arg[%d]: %s\n", i, args[i]);
    }
    printf("---------------------------\n\n");
    */
    }

int main(void) {
    char buffer[MAX_INPUT];
    char *args[MAX_ARGS];
    int running = 1;

    while (running) {
        display_prompt();
        
        int bytes = read_input(buffer);
        if (bytes <= 0) {
            break;
        }

        running = parse_input(buffer, args);

        if (running && args[0] != NULL) {
            execute_command(args);
        }
    }

    return 0;
}