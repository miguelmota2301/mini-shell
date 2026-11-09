#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "parser.h"
#include "executor.h"

int main(void) {
    char buffer[MAX_INPUT];
    Pipeline pipeline;
    int running = 1;

    while (running) {
        if (display_prompt() < 0) {
            return EXIT_FAILURE;
        }
        
        int bytes = read_input(buffer);
        if (bytes < 0) {
            return EXIT_FAILURE;
        }
        if (bytes == 0) {
            break;
        }

        int parsed = parse_input(buffer, &pipeline);
        if (parsed < 0) {
            continue;
        }
        running = parsed;

        if (running && pipeline.count > 0) {
            execute_pipeline(&pipeline);
        }
    }

    return 0;
}
