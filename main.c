#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "parser.h"
#include "executor.h"

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