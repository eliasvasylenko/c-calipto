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

#include "c-calipto/stringref.h"
#include "c-calipto/sexpr.h"
#include "c-calipto/stream.h"
#include "c-calipto/scanner.h"
#include "c-calipto/reader.h"
#include "c-calipto/interpreter.h"

static s_expr system_fail;
static s_expr system_cont;
static s_expr data_true;
static s_expr data_false;

bool system_exit(s_expr* args, s_bound_expr* b) {
	return false;
}

bool system_in(s_expr* args, s_bound_expr* b) {
	s_expr fail = args[0];
	s_expr cont = args[1];
	*b = (s_bound_expr){ s_nil(), s_alloc_bindings(NULL, 0, NULL) };
	return true;
}

bool system_out(s_expr* args, s_bound_expr* b) {
	s_expr string = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];
	
	if (string.type != STRING) {
		s_binding bindings[] = { { system_fail, fail } };
		*b = (s_bound_expr){
			s_cons(system_fail, s_nil()),
			s_alloc_bindings(NULL, 1, bindings)
		};
		s_free(system_fail);

	} else {
		u_printf("%S", string.string);

		s_binding bindings[] = { { system_cont, cont } };
		*b = (s_bound_expr){
			s_cons(system_cont, s_nil()),
			s_alloc_bindings(NULL, 1, bindings)
		};
		s_free(system_cont);
	}
	return true;
}

bool data_cons(s_expr* args, s_bound_expr* b) {
	*b = (s_bound_expr){ s_nil(), NULL };
	return true;
}

bool data_des(s_expr* args, s_bound_expr* b) {
	*b = (s_bound_expr){ s_nil(), NULL };
	return true;
}

bool data_eq(s_expr* args, s_bound_expr* b) {
	s_expr e_a = args[0];
	s_expr e_b = args[1];
	s_expr f = args[2];
	s_expr t = args[3];

	if (s_eq(e_a, e_b)) {
		s_binding bindings[] = { { data_true, t } };
		*b = (s_bound_expr){
			s_cons(data_true, s_nil()),
			s_alloc_bindings(NULL, 1, bindings)
		};
		s_free(data_true);
	} else {
		s_binding bindings[] = { { data_false, f } };
		*b = (s_bound_expr){
			s_cons(data_false, s_nil()),
			s_alloc_bindings(NULL, 1, bindings)
		};
		s_free(data_false);
	}

	return true;
}

s_binding builtin_binding(UChar* ns, UChar* n, int32_t c, bool (*f)(s_expr* args, s_bound_expr* b)) {
	s_expr sym = s_symbol(u_strref(ns), u_strref(n));
	s_expr bi = s_builtin(u_strref(n), c, f);
	return (s_binding){ sym, bi };
}

s_bindings builtin_bindings() {
	system_fail = s_symbol(u_strref(u"system"), u_strref(u"fail"));
	system_cont = s_symbol(u_strref(u"system"), u_strref(u"cont"));
	data_true = s_symbol(u_strref(u"data"), u_strref(u"true"));
	data_false = s_symbol(u_strref(u"data"), u_strref(u"false"));
	
	s_binding bindings[] = {
		builtin_binding(u"system", u"exit", 0, *system_exit),
		builtin_binding(u"system", u"in", 2, *system_in),
		builtin_binding(u"system", u"out", 3, *system_out),
		builtin_binding(u"system", u"err", 3, *system_out),
		builtin_binding(u"data", u"cons", 3, *data_cons),
		builtin_binding(u"data", u"des", 3, *data_des),
		builtin_binding(u"data", u"eq", 4, *data_eq),
	};

	size_t c = sizeof(bindings)/sizeof(s_binding);
	s_bindings b = s_alloc_bindings(NULL, c, bindings);
	for (int i = 0; i < c; i++) {
		s_free(bindings[i].name);
		s_free(bindings[i].value);
	}
	return b;
}

int main(int argc, char** argv) {
	setlocale(LC_ALL, "");
	UErrorCode error = 0;
	UConverter* char_conv = ucnv_open(NULL, &error);

	s_expr args = s_nil();
	for (int i = argc - 1; i >= 0; i--) {
		s_expr arg = s_string(c_strref(char_conv, argv[i]));
		s_expr rest = args;

		args = s_cons(arg, rest);

		s_free(arg);
		s_free(rest);
	}

	s_dump(args);

	UFILE* f = u_fopen("./bootstrap.cal", "r", NULL, NULL);
	stream* st = open_file_stream(f);
	scanner* sc = open_scanner(st);
	reader* r = open_reader(sc);

	s_expr e;
	if (read(r, &e)) {
		s_bindings b = builtin_bindings();
		eval(e, b);
		s_free_bindings(b);

		s_free(e);
	} else {
		printf("Failed to read bootstrap file!");
	}

	close_reader(r);
	close_scanner(sc);
	close_stream(st);
	u_fclose(f);

	s_free(args);

	ucnv_close(char_conv);
	return 0;
}
