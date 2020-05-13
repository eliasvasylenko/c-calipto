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
static s_expr data_fail;
static s_expr data_cont;
static s_expr data_true;
static s_expr data_false;
static s_expr data_car;
static s_expr data_cdr;
static s_expr system_args;

void no_op(void* d) {}

bool system_exit(s_bound_expr* b, s_expr* args, void* d) {
	return false;
}

bool system_in(s_bound_expr* b, s_expr* args, void* d) {
	s_expr fail = args[0];
	s_expr cont = args[1];
	*b = (s_bound_expr){ s_nil(), s_alloc_bindings(NULL, 0, NULL) };
	return true;
}

typedef struct system_out_data {
	FILE* file;
	s_expr* next;
	UChar* text;
} system_out_data;

bool system_out(s_bound_expr* b, s_expr* args, void* d) {
	s_expr string = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];
	
	system_out_data data = *(system_out_data*)d;

	if (string.type != STRING) {
		s_binding bindings[] = { { system_fail, fail } };
		*b = (s_bound_expr){
			s_cons(system_fail, s_nil()),
			s_alloc_bindings(NULL, 1, bindings)
		};

	} else if (data.next == NULL) {
		u_fprintf(data.file, "%S", string.string);

		s_binding bindings[] = { { system_cont, cont } };
		*b = (s_bound_expr){
			s_cons(system_cont, s_nil()),
			s_alloc_bindings(NULL, 1, bindings)
		};

	} else {


	}
	return true;
}

bool data_cons(s_bound_expr* b, s_expr* args, void* d) {
	*b = (s_bound_expr){ s_nil(), NULL };
	return true;
}

bool data_des(s_bound_expr* b, s_expr* args, void* d) {
	s_expr e = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];

	if (s_atom(e)) {
		s_binding bindings[] = { { data_fail, fail } };
		*b = (s_bound_expr){
			s_cons(data_fail, s_nil()),
			s_alloc_bindings(NULL, 1, bindings)
		};

	} else {
		s_expr tail = s_nil();
		s_expr prev = tail;
		
		tail = s_cons(data_cdr, tail);
		s_free(prev);
		prev = tail;

		tail = s_cons(data_car, tail);
		s_free(prev);
		prev = tail;

		tail = s_cons(data_cont, tail);
		s_free(prev);

		s_expr cdr = s_cdr(e);
		s_expr car = s_car(e);
		s_binding bindings[] = {
			{ data_cont, cont },
			{ data_car, car },
			{ data_cdr, cdr }
		};
		*b = (s_bound_expr){
			tail,
			s_alloc_bindings(NULL, 3, bindings)
		};
		s_free(car);
		s_free(cdr);
	}
	return true;
}

bool data_eq(s_bound_expr* b, s_expr* args, void* d) {
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

s_binding builtin_binding(UChar* ns, UChar* n, int32_t c,
		bool (*a)(s_bound_expr* b, s_expr* args, void* d),
		void (*f)(void* d),
		void* data) {
	s_expr sym = s_symbol(u_strref(ns), u_strref(n));
	s_expr bi = s_builtin(u_strref(n), c, a, f, data);
	return (s_binding){ sym, bi };
}

s_bindings builtin_bindings(s_expr args) {
	system_fail = s_symbol(u_strref(u"system"), u_strref(u"fail"));
	system_cont = s_symbol(u_strref(u"system"), u_strref(u"cont"));
	data_fail = s_symbol(u_strref(u"data"), u_strref(u"fail"));
	data_cont = s_symbol(u_strref(u"data"), u_strref(u"cont"));
	data_true = s_symbol(u_strref(u"data"), u_strref(u"true"));
	data_false = s_symbol(u_strref(u"data"), u_strref(u"false"));
	data_car = s_symbol(u_strref(u"data"), u_strref(u"car"));
	data_cdr = s_symbol(u_strref(u"data"), u_strref(u"cdr"));
	system_args = s_symbol(u_strref(u"system"), u_strref(u"arguments"));

	s_ref(args);

	system_out_data* s_o = malloc(sizeof(system_out_data));
	*s_o = (system_out_data){ stdout, NULL, NULL };

	system_out_data* s_e = malloc(sizeof(system_out_data));
	*s_e = (system_out_data){ stderr, NULL, NULL };

	s_binding bindings[] = {
		builtin_binding(u"system", u"exit", 0, *system_exit, *no_op, NULL),
		builtin_binding(u"system", u"in", 2, *system_in, *system_in_free s_i),
		builtin_binding(u"system", u"out", 3, *system_out, *system_out_free, s_o),
		builtin_binding(u"system", u"err", 3, *system_out, *system_out_free, s_e),
		builtin_binding(u"data", u"cons", 3, *data_cons, *no_op, NULL),
		builtin_binding(u"data", u"des", 3, *data_des, *no_op, NULL),
		builtin_binding(u"data", u"eq", 4, *data_eq, *no_op, NULL),
		(s_binding){ system_args, args }
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

	UFILE* f = u_fopen("./bootstrap.cal", "r", NULL, NULL);
	stream* st = open_file_stream(f);
	scanner* sc = open_scanner(st);
	reader* r = open_reader(sc);

	s_expr e;
	if (read(r, &e)) {
		s_bindings b = builtin_bindings(args);
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
