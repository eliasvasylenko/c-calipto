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
#include "c-calipto/idtrie.h"
#include "c-calipto/sexpr.h"
#include "c-calipto/stream.h"
#include "c-calipto/scanner.h"
#include "c-calipto/reader.h"
#include "c-calipto/interpreter.h"
#include "c-calipto/io.h"

static s_term* terms;
static UConverter* char_conv;

void no_op(void* d) {}

s_expr no_rep(void* d) { return s_list(0, NULL); }

bool system_exit(s_statement* b, s_expr* args, void* d) {
	return false;
}


bool data_cons(s_statement* b, s_expr* args, void* d) {
	s_expr car = args[0];
	s_expr cdr = args[1];
	s_expr cont = args[2];

	s_expr* bindings = malloc(sizeof(s_expr) * 2);
	bindings[0] = cont;
	bindings[1] = s_cons(car, cdr);
	*b = (s_statement){ 2, terms, bindings };

	return true;
}

bool data_des(s_statement* b, s_expr* args, void* d) {
	s_expr e = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];

	if (s_atom(e)) {
		s_expr* bindings = malloc(sizeof(s_expr) * 1);
		bindings[0] = fail;
		*b = (s_statement){ 1, terms, bindings };

	} else {
		s_expr* bindings = malloc(sizeof(s_expr) * 3);
		bindings[0] = cont;
		bindings[1] = s_car(e);
		bindings[2] = s_cdr(e);
		*b = (s_statement){ 3, terms, bindings };
	}
	return true;
}

bool data_eq(s_statement* b, s_expr* args, void* d) {
	s_expr e_a = args[0];
	s_expr e_b = args[1];
	s_expr f = args[2];
	s_expr t = args[3];

	if (s_eq(e_a, e_b)) {
		s_expr* bindings = malloc(sizeof(s_expr) * 1);
		bindings[0] = t;
		*b = (s_statement){ 1, terms, bindings };
	} else {
		s_expr* bindings = malloc(sizeof(s_expr) * 1);
		bindings[0] = f;
		*b = (s_statement){ 1, terms, bindings };
	}

	return true;
}

s_bound_arguments bind_root_arguments(s_expr args) {
	s_expr builtins[] = {
		cal_open_scanner(
				u_finit(stdin, NULL, NULL),
				s_string(u_strref(u"stdin"))),
		cal_open_printer(
				u_finit(stdout, NULL, NULL),
				s_string(u_strref(u"stdout"))),
		cal_open_printer(
				u_finit(stdout, NULL, NULL),
				s_string(u_strref(u"stderr"))),
		s_builtin(s_symbol(s_system.p, u_strref(u"err")).p,
				*no_rep, 3, *system_out, *system_out_free, s_e),
		s_builtin(s_symbol(s_system.p, u_strref(u"exit")).p,
				*no_rep, 0, *system_exit, *no_op, NULL),
		s_builtin(s_symbol(s_data.p, u_strref(u"cons")).p,
				*no_rep, 3, *data_cons, *no_op, NULL),
		s_builtin(s_symbol(s_data.p, u_strref(u"des")).p,
				*no_rep, 3, *data_des, *no_op, NULL),
		s_builtin(s_symbol(s_data.p, u_strref(u"eq")).p,
				*no_rep, 4, *data_eq, *no_op, NULL),
	};

	int32_t builtin_count = sizeof(builtins) / sizeof(s_expr);

	s_expr_ref** parameters = malloc(sizeof(s_expr_ref*) * builtin_count);
	s_expr* arguments = malloc(sizeof(s_expr) * builtin_count);

	for (int i = 0; i < builtin_count; i++) {
		s_free(SYMBOL, builtins[i].p->builtin.name);
		parameters[i] = builtins[i].p->builtin.name;
		arguments[i] = builtins[i];
	}

	s_expr_ref* system_args = s_symbol(s_system.p, u_strref(u"arguments")).p;

	return s_bind_arguments(parameters, arguments);
}

s_expr read_arg(void* arg) {
	return s_string(c_strref(char_conv, *(char**)arg));
}

int main(int argc, char** argv) {
	setlocale(LC_ALL, "");
	UErrorCode error = 0;
	char_conv = ucnv_open(NULL, &error);

	s_init();

	terms = (s_term[]){
		s_variable(0),
		s_variable(1),
		s_variable(2),
		s_variable(3)
	};

	s_expr args = s_list_of(argc, (void**)argv, read_arg);

	UFILE* f = u_fopen("./bootstrap.cal", "r", NULL, NULL);
	stream* st = open_file_stream(f);
	scanner* sc = open_scanner(st);
	reader* r = open_reader(sc);

	s_expr e;
	if (read(r, &e)) {
		s_bound_arguments b = bind_root_arguments(args);
		s_eval(e, b);
		s_unbind_arguments(b);

		s_dealias(e);
	} else {
		printf("Failed to read bootstrap file!");
	}

	close_reader(r);
	close_scanner(sc);
	close_stream(st);
	u_fclose(f);

	s_dealias(args);

	s_close();

	ucnv_close(char_conv);
	return 0;
}
