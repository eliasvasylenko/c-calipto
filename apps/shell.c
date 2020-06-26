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

int run(ovs_expr e, ovs_expr args) {
	ovs_expr_ref* data = ovs_symbol(NULL, ovio_u_strref(u"data")).p;
	ovs_expr_ref* system = ovs_symbol(NULL, ovio_u_strref(u"system")).p;
	const ovs_expr_ref* parameters[] = {
		ovs_symbol(system, ovio_u_strref(u"args")).p,
		ovs_symbol(system, ovio_u_strref(u"exit")).p,
		ovs_symbol(data, ovio_u_strref(u"cons")).p,
		ovs_symbol(data, ovio_u_strref(u"des")).p,
		ovs_symbol(data, ovio_u_strref(u"eq")).p,
		ovs_symbol(system, ovio_u_strref(u"in")).p,
		ovs_symbol(system, ovio_u_strref(u"out")).p,
		ovs_symbol(system, ovio_u_strref(u"err")).p
	};
	ovs_free(OVS_SYMBOL, data);
	ovs_free(OVS_SYMBOL, system);

	ovru_statement s;
	if (ovru_compile(&s, e, sizeof(parameters) / sizeof(ovs_expr_ref*), parameters) == OVRU_SUCCESS) {
		const ovs_expr arguments[] = {
			args,
			ovru_exit(),
			ovru_cons(),
			ovru_des(),
			ovru_eq(),
			ovru_open_scanner(
					u_finit(stdin, NULL, NULL),
					ovs_string(ovio_u_strref(u"stdin"))),
			ovru_open_printer(
					u_finit(stdout, NULL, NULL),
					ovs_string(ovio_u_strref(u"stdout"))),
			ovru_open_printer(
					u_finit(stderr, NULL, NULL),
					ovs_string(ovio_u_strref(u"stderr")))
		};

		return ovru_eval(s, arguments);
	} else {
		return 3;
	}
}

int run_bootstrap(ovs_expr args) {
	UFILE* f = u_fopen("./data/bootstrap.ov", "r", NULL, NULL);

	if (f == NULL) {
		return 1;
	}

	ovio_stream* st = ovio_open_file_stream(f);
	ovio_scanner* sc = ovio_open_scanner(st);
	ovda_reader* r = ovda_open_reader(sc);

	ovs_expr e;
	int result;
	if (ovda_read(r, &e) == OVDA_SUCCESS) {
		ovs_dump(e);
		result = run(e, args);

		ovs_dealias(e);
	} else {
		result = 2;
	}

	ovda_close_reader(r);
	ovio_close_scanner(sc);
	ovio_close_stream(st);
	u_fclose(f);

	return result;
}

ovs_expr read_arg(void* arg) {
	return ovs_string(ovio_c_strref(char_conv, (char*)arg));
}

int main(int argc, char** argv) {
	setlocale(LC_ALL, "");
	UErrorCode error = 0;
	char_conv = ucnv_open(NULL, &error);

	ovs_init();
	ovs_expr args = ovs_list_of(argc, (void**)argv, read_arg);

	int result = run_bootstrap(args);

	ovs_dealias(args);
	ovs_close();

	ucnv_close(char_conv);

	printf("Returning with %i\n", result);

	return result;
}
