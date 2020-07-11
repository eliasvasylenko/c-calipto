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

#include "c-ohvu/io/stringref.h"
#include "c-ohvu/io/stream.h"
#include "c-ohvu/io/scanner.h"
#include "c-ohvu/data/bdtrie.h"
#include "c-ohvu/data/sexpr.h"
#include "c-ohvu/data/reader.h"
#include "c-ohvu/runtime/interpreter.h"
#include "c-ohvu/runtime/builtins.h"

static UConverter* char_conv;
static ovs_context context;

int run(ovs_expr e, ovs_expr args) {
	const ovs_expr_ref* parameters[] = {
		ovs_symbol(context.root_tables + OVS_SYSTEM, u_strlen("args"), u"args").p,
		ovs_symbol(context.root_tables + OVS_SYSTEM, u_strlen(u"exit"), u"exit").p,
		ovs_symbol(context.root_tables + OVS_DATA, u_strlen(u"cons"), u"cons").p,
		ovs_symbol(context.root_tables + OVS_DATA, u_strlen(u"des"), u"des").p,
		ovs_symbol(context.root_tables + OVS_DATA, u_strlen(u"eq"), u"eq").p,
		ovs_symbol(context.root_tables + OVS_SYSTEM, u_strlen(u"in"), u"in").p,
		ovs_symbol(context.root_tables + OVS_SYSTEM, u_strlen(u"out"), u"out").p,
		ovs_symbol(context.root_tables + OVS_SYSTEM, u_strlen(u"err"), u"err").p
	};

	uint32_t c = sizeof(parameters) / sizeof(ovs_expr_ref*);

	ovru_statement s;
	ovru_result r = ovru_compile(&s, e, c, (const ovs_expr_ref**)parameters);
	if (r != OVRU_SUCCESS) {
		for (int i = 0; i < c; i++) {
			ovs_free(OVS_SYMBOL, parameters[i]);
		}

		return r;
	}

	ovs_expr arguments[] = {
		ovs_alias(args),
		ovru_exit(&context),
		ovru_cons(&context, context.root_tables + OVS_UNQUALIFIED),
		ovru_des(&context, context.root_tables + OVS_UNQUALIFIED),
		ovru_eq(&context),
		ovru_open_scanner(
				&context,
				u_finit(stdin, NULL, NULL),
				u"stdin"),
		ovru_open_printer(
				&context,
				u_finit(stdout, NULL, NULL),
				u"stdout"),
		ovru_open_printer(
				&context,
				u_finit(stderr, NULL, NULL),
				u"stderr")
	};

	r = ovru_eval(&context, s, arguments);

	ovru_free(s);

	for (int i = 0; i < c; i++) {
		ovs_free(OVS_SYMBOL, parameters[i]);
		ovs_dealias(arguments[i]);
	}

	return r;
}

int run_bootstrap(ovs_expr args) {
	UFILE* f = u_fopen("./data/bootstrap.ov", "r", NULL, NULL);

	if (f == NULL) {
		return 123456;
	}

	ovio_stream* st = ovio_open_file_stream(f);
	ovio_scanner* sc = ovio_open_scanner(st);
	ovda_reader* r = ovda_open_reader(sc, &context);

	ovs_expr e;
	int result;
	if (ovda_read(r, &e) == OVDA_SUCCESS) {
		ovs_dump(e);
		result = run(e, args);

		ovs_dealias(e);
	} else {
		result = 123457;
	}

	ovda_close_reader(r);
	ovio_close_scanner(sc);
	ovio_close_stream(st);
	u_fclose(f);

	return result;
}

ovs_expr read_arg(const void* arg) {
	return ovs_cstring(char_conv, (char*)arg);
}

int main(int argc, char** argv) {
	setlocale(LC_ALL, "");
	UErrorCode error = 0;
	char_conv = ucnv_open(NULL, &error);

	context = ovs_init();
	ovs_expr args = ovs_list_of(context.root_tables + OVS_UNQUALIFIED, argc, (void**)argv, read_arg);

	int result = run_bootstrap(args);

	ovs_dealias(args);
	ovs_close(&context);

	ucnv_close(char_conv);

	printf("Returning with %i\n", result);

	return result;
}

