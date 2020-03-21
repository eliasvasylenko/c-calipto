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
	sexpr* args = sexpr_symbol(U"system", U"nil");
	for (int i = argc - 1; i >= 0; i--) {
		scanner_handle* s = open_string_scanner(argv[i]);
		reader_handle* r = open_reader(s);
		sexpr* arg = read(r);
		close_scanner(s);
		close_reader(r);

		sexpr* rest = args;

		args = sexpr_cons(arg, rest);

		sexpr_free(arg);
		sexpr_free(rest);
	}

	sexpr_dump(args);

	FILE* bsf = fopen("./bootstrap.cal", "r");
	scanner_handle* bss = open_file_scanner(bsf);
	reader_handle* bsr = open_reader(bss);

	;

	close_reader(bsr);
	close_scanner(bss);
	fclose(bsf);

	// TODO evaluate bootstrap file

	sexpr_free(args);
	return 0;
}
