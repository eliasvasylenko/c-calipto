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

static s_expr builtin;
static s_expr builtin_fail;
static s_expr builtin_cont;
static s_expr builtin_true;
static s_expr builtin_false;
static s_expr builtin_car;
static s_expr builtin_cdr;
static s_expr system_args;

static UConverter* char_conv;

void no_op(void* d) {}

bool system_exit(s_expr_ref** b, s_expr* args, void* d) {
	return false;
}

typedef struct system_in_data {
	UFILE* file;
	s_expr* next;
	UChar* text;
} system_in_data;

bool system_in(s_expr_ref** b, s_expr* args, void* d) {
	s_expr fail = args[0];
	s_expr cont = args[1];
	*b = s_instruction(system_in_fail, fail).p;
	return true;
}

void system_in_free(void* d) {
	system_in_data data = *(system_in_data*)d;

	if (data.next) {
		s_free(*data.next);
		free(data.next);

		if (data.text) {
			free(data.text);
		}
	} else if (data.file) {
		u_fclose(data.file);
	}

	free(d);
}

static s_expr system_out_cont;
static s_expr system_out_fail;

typedef struct system_out_data {
	UFILE* file;
	s_expr* next;
	UChar* text;
} system_out_data;

bool system_out(s_expr_ref** b, s_expr* args, void* d) {
	s_expr string = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];
	
	system_out_data data = *(system_out_data*)d;

	if (string.type != STRING) {
		s_expr bindings[] = { fail };
		*b = s_instruction(system_out_fail.p, bindings);

	} else if (data.next == NULL) {
		u_fprintf(data.file, "%S", string.string);

		s_expr bindings[] = { cont };
		*b = s_instruction(system_out_cont.p, bindings);

	} else {


	}
	return true;
}

void system_out_free(void* d) {
	system_out_data data = *(system_out_data*)d;

	if (data.next) {
		s_free(*data.next);
		free(data.next);

		if (data.text) {
			free(data.text);
		}
	} else if (data.file) {
		u_fclose(data.file);
	}

	free(d);
}

bool data_cons(s_expr_ref** b, s_expr* args, void* d) {
	*b = (s_bound_expr){ s_nil(), NULL };
	return true;
}

bool data_des(s_expr_ref** b, s_expr* args, void* d) {
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

static s_expr data_eq_true = s_statement(1, &data_true.p, 1, &s_variable(0));
static s_expr data_eq_false = s_statement(1, &data_false.p, 1, &s_variable(0));

bool data_eq(s_expr_ref** b, s_expr* args, void* d) {
	s_expr e_a = args[0];
	s_expr e_b = args[1];
	s_expr f = args[2];
	s_expr t = args[3];

	if (s_eq(e_a, e_b)) {
		s_expr bindings[] = { t };
		*b = s_instruction(data_eq_true, bindings).p;
	} else {
		s_expr bindings[] = { f };
		*b = s_instruction(data_eq_false, bindings).p;
	}

	return true;
}

s_binding builtin_binding(UChar* ns, UChar* n, int32_t c,
		bool (*a)(s_expr_ref** b, s_expr* args, void* d),
		void (*f)(void* d),
		void* data) {
	s_expr sym = s_symbol(u_strref(ns), u_strref(n));
	s_expr bi = s_builtin(u_strref(n), c, a, f, data);
	return (s_binding){ sym, bi };
}

s_bound_arguments builtin_bindings(s_expr args) {
	builtin = s_symbol(s_nil(), u_strref(u"builtin"));
	builtin_fail = s_symbol(builtin, u_strref(u"fail"));
	builtin_cont = s_symbol(builtin, u_strref(u"cont"));
	builtin_true = s_symbol(builtin, u_strref(u"true"));
	builtin_false = s_symbol(builtin, u_strref(u"false"));
	builtin_car = s_symbol(builtin, u_strref(u"car"));
	builtin_cdr = s_symbol(builtin, u_strref(u"cdr"));

	system_args = s_symbol(builtin, u_strref(u"arguments"));


	system_in_data* s_i = malloc(sizeof(system_in_data));
	*s_i = (system_in_data){ u_finit(stdin, NULL, NULL), NULL, NULL };

	system_out_fail = s_statement(1, &builtin_fail.p, 1, &s_variable(0));
	system_out_cont = s_statement(1, &builtin_cont.p, 1, &s_variable(0));

	system_out_data* s_o = malloc(sizeof(system_out_data));
	*s_o = (system_out_data){ u_finit(stdout, NULL, NULL), NULL, NULL };

	system_out_data* s_e = malloc(sizeof(system_out_data));
	*s_e = (system_out_data){ u_finit(stderr, NULL, NULL), NULL, NULL };

	s_binding builtins[] = {
		builtin_binding(u"system", u"exit",
				*no_rep, 0, *system_exit, *no_op, NULL),
		builtin_binding(u"system", u"in",
				*no_rep, 2, *system_in, *system_in_free, s_i),
		builtin_binding(u"system", u"out",
				*no_rep, 3, *system_out, *system_out_free, s_o),
		builtin_binding(u"system", u"err",
				*no_rep, 3, *system_out, *system_out_free, s_e),
		builtin_binding(u"data", u"cons",
				*no_rep, 3, *data_cons, *no_op, NULL),
		builtin_binding(u"data", u"des",
				*no_rep, 3, *data_des, *no_op, NULL),
		builtin_binding(u"data", u"eq",
				*no_rep, 4, *data_eq, *no_op, NULL),
		(s_binding){ system_args, args }
	};

	int32_t builtin_count = sizeof(bindings) / sizeof(s_expr);

	s_expr_ref* parameters[] = malloc(sizeof(s_expr_ref*) * builtin_count);
	s_expr arguments[] = malloc(sizeof(s_expr) * builtin_count);

	for (int i = 0; i < binding_count; i++) {
		parameters[i] = s_symbol(builtin, builtins[i]->builtin.name);
		arguments[i] = (s_expr){ BUILTIN, builtins[i] };
	}

	s_bindings bindings = s_prepare_bindings(parameters);
	return s_bind_arguments(bindings, arguments);
}

int main(int argc, char** argv) {
	setlocale(LC_ALL, "");
	UErrorCode error = 0;
	char_conv = ucnv_open(NULL, &error);

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
