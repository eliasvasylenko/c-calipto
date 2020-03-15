#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>

#include <sexpr.h>
#include <scanner.h>
#include <reader.h>

bool always(char32_t c) {
	return true;
}

int main(int argc, char** argv) {
	sexpr* args = sexpr_symbol(U"system:nil");
	for (int i = 0; i < argc; i++) {
		scanner_handle* h = scan_string(argv[i]);
		advance_input_while(h, always);
		size_t size = sizeof(char32_t) * (input_position(h) - buffer_position(h) + 1);
		char32_t* arg32 = malloc(size);
		take_buffer(h, arg32);
		sexpr* arg = sexpr_symbol(arg32);
		free(arg32);
		close_scanner(h);

		sexpr* rest = args;

		args = sexpr_cons(arg, rest);

		sexpr_free(arg);
		sexpr_free(rest);
	}

	FILE* bsf = fopen("./bootstrap.cal", "r");
	scanner_handle* bs = scan_file(bsf);

	// TODO open reader, read data

	close_scanner(bs);
	fclose(bsf);

	// TODO evaluate bootstrap file

	sexpr_free(args);
	return 0;
}
