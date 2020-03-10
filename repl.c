#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>

#include <scanner.h>
#include <reader.h>
#include <sexpr.h>

int main(int argc, char** argv) {
        sexpr* args = sexpr_symbol(U"system:nil");
        for (int i = 0; i < argc; i++) {
                sexpr* rest = args;

                char32_t* arg32 = utf8to32(argv[i]);
                sexpr* arg = sexpr_symbol(arg32);
                free(arg32);

                args = sexpr_cons(arg, rest);

                sexpr_free(arg);
                sexpr_free(rest);
        }

        /*
         * TODO read and eval bootstrap file
         */

        sexpr_free(args);
        return 0;
}
