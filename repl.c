#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <uchar.h>
typedef char32_t character;

#include <scanner.h>
#include <reader.h>
#include <sexpr.h>

#include <readline/readline.h>
#include <readline/history.h>

int main(int argc, char** argv) {
        puts("Calipto Language Version 0");
        puts("C-Calipto Interpreter Version 0.0.1");
        puts("Call the `exit` function to exit\n");

        while (1) {
                char* input = readline("calipto> ");
                add_history(input);
                printf("Did you say %s?\n", input);
                free(input);
        }

        return 0;
}
