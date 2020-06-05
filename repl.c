#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>
#include <string.h>
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
#include "c-calipto/builtins.h"

static s_term* terms;
static UConverter* char_conv;

void run(s_expr e, s_expr args) {
	s_expr_ref* data = s_symbol(NULL, u_strref(u"data")).p;
	s_expr_ref* system = s_symbol(NULL, u_strref(u"system")).p;
	const s_expr_ref* parameters[] = {
		s_symbol(system, u_strref(u"args")).p,
		s_symbol(system, u_strref(u"exit")).p,
		s_symbol(data, u_strref(u"cons")).p,
		s_symbol(data, u_strref(u"des")).p,
		s_symbol(data, u_strref(u"eq")).p,
		s_symbol(system, u_strref(u"in")).p,
		s_symbol(system, u_strref(u"out")).p,
		s_symbol(system, u_strref(u"err")).p
	};
	s_free(SYMBOL, data);
	s_free(SYMBOL, system);

	s_statement s = s_compile(e, sizeof(parameters) / sizeof(s_expr_ref*), parameters);

	const s_expr arguments[] = {
		args,
		cal_exit(),
		cal_cons(),
		cal_des(),
		cal_eq(),
		cal_open_scanner(
				u_finit(stdin, NULL, NULL),
				s_string(u_strref(u"stdin"))),
		cal_open_printer(
				u_finit(stdout, NULL, NULL),
				s_string(u_strref(u"stdout"))),
		cal_open_printer(
				u_finit(stderr, NULL, NULL),
				s_string(u_strref(u"stderr")))
	};

	s_eval(s, arguments);
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
		{ .type=VARIABLE, .variable=0 },
		{ .type=VARIABLE, .variable=1 },
		{ .type=VARIABLE, .variable=2 },
		{ .type=VARIABLE, .variable=3 },
	};

	s_expr args = s_list_of(argc, (void**)argv, read_arg);

	UFILE* f = u_fopen("./bootstrap.cal", "r", NULL, NULL);
	stream* st = open_file_stream(f);
	scanner* sc = open_scanner(st);
	reader* r = open_reader(sc);

	s_expr e;
	if (read(r, &e)) {
		run(e, args);

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
