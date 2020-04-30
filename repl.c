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

s_bound_expr system_exit(s_expr* args) {
	return (s_bound_expr){ s_alloc_nil(), NULL };
}

s_bound_expr data_cons(s_expr* args) {
	return (s_bound_expr){ s_alloc_nil(), NULL };
}

s_bound_expr data_des(s_expr* args) {
	return (s_bound_expr){ s_alloc_nil(), NULL };
}

s_bound_expr data_eq(s_expr* args) {
	return (s_bound_expr){ s_alloc_nil(), NULL };
}

s_binding builtin_binding(UChar* ns, UChar* n, int32_t c, s_bound_expr (*f)(s_expr* args)) {
	s_symbol sym = { s_u_strcpy(ns), s_u_strcpy(n) };
	s_builtin bi = { s_u_strcpy(n), c, f };
	s_binding b = { s_alloc_symbol(sym), s_alloc_builtin(bi) };
	return b;
}

s_bindings* builtin_bindings() {
	s_binding bindings[] = {
		builtin_binding(u"system", u"exit", 0, *system_exit),
		builtin_binding(u"data", u"cons", 3, *data_cons),
		builtin_binding(u"data", u"des", 3, *data_des),
		builtin_binding(u"data", u"eq", 4, *data_eq),
	};

	size_t c = sizeof(bindings)/sizeof(s_binding);
	return s_bindings_alloc(NULL, c, bindings);
}

int main(int argc, char** argv) {
	setlocale(LC_ALL, "");
	UErrorCode error = 0;
	UConverter* char_conv = ucnv_open(NULL, &error);

	s_expr args = s_alloc_nil();
	for (int i = argc - 1; i >= 0; i--) {
		s_expr arg = s_alloc_string(s_strcpy(char_conv, argv[i]));
		s_expr rest = args;

		s_cons c = { arg, rest };
		args = s_alloc_cons(c);

		s_free(arg);
		s_free(rest);
	}

	s_dump(args);

	UFILE* f = u_fopen("./bootstrap.cal", "r", NULL, NULL);
	stream* st = open_file_stream(f);
	scanner* sc = open_scanner(st);
	reader* r = open_reader(sc);

	s_expr e = read(r);
	s_dump(e);
	
	s_bindings* b = builtin_bindings();
	eval(e, b);
	s_bindings_free(b);

	s_free(e);

	close_reader(r);
	close_scanner(sc);
	close_stream(st);
	u_fclose(f);

	s_free(args);

	ucnv_close(char_conv);
	return 0;
}
