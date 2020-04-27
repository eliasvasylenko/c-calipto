#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>
#include <locale.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/ucnv.h>
#include <unicode/ustdio.h>

#include "c-calipto/sexpr.h"
#include "c-calipto/stream.h"
#include "c-calipto/scanner.h"
#include "c-calipto/reader.h"
#include "c-calipto/interpreter.h"

bindings* default_bindings() {
	sexpr* exit = sexpr_usymbol(u"system", u"exit");
	sexpr* lambda = sexpr_usymbol(u"function", u"lambda");
	binding bindings[] = {
		{ exit, exit },
		{ lambda, lambda }
	};
	sexpr_free(exit);
	sexpr_free(lambda);

	size_t c = sizeof(bindings)/sizeof(binding);
	return collect_bindings(NULL, c, bindings);
}

int main(int argc, char** argv) {
	setlocale(LC_ALL, "");
	UErrorCode error = 0;
	UConverter* char_conv = ucnv_open(NULL, &error);

	sexpr* args = sexpr_nil();
	for (int i = argc - 1; i >= 0; i--) {
		sexpr* arg = sexpr_string(char_conv, argv[i]);
		sexpr* rest = args;

		args = sexpr_cons(arg, rest);

		sexpr_free(arg);
		sexpr_free(rest);
	}

	sexpr_dump(args);

	UFILE* f = u_fopen("./bootstrap.cal", "r", NULL, NULL);
	stream* st = open_file_stream(f);
	scanner* sc = open_scanner(st);
	reader* r = open_reader(sc);

	sexpr* e = read(r);
	sexpr_dump(e);
	
	bindings* b = default_bindings();
	sexpr* d = eval(e, b);
	free(b);

	sexpr_dump(d);
	sexpr_free(d);
	sexpr_free(e);

	close_reader(r);
	close_scanner(sc);
	close_stream(st);
	u_fclose(f);

	sexpr_free(args);

	ucnv_close(char_conv);
	return 0;
}
