#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>

#include "c-calipto/sexpr.h"
#include "c-calipto/scanner.h"
#include "c-calipto/reader.h"

bool always(char32_t c) {
	return true;
}

int main(int argc, char** argv) {
	sexpr* args = sexpr_symbol(u"system", u"nil");
	for (int i = argc - 1; i >= 0; i--) {
		stream* st = open_string_stream(argv[i]);
		scanner* sc = open_scanner(st);
		reader* r = open_reader(sc);
		sexpr* arg = read(r);
		close_reader(r);
		close_scanner(sc);
		close_stream(st);

		sexpr* rest = args;

		args = sexpr_cons(arg, rest);

		sexpr_free(arg);
		sexpr_free(rest);
	}

	sexpr_dump(args);

	FILE* f = fopen("./bootstrap.cal", "r");
	stream* st = open_file_stream(f);
	scanner* sc = open_scanner(st);
	reader* r = open_reader(sc);

	;

	close_reader(r);
	close_scanner(sc);
	close_stream(st);
	fclose(f);

	// TODO evaluate bootstrap file

	sexpr_free(args);
	return 0;
}
