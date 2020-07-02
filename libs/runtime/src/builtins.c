#include <stdbool.h>
#include <stdlib.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>
#include <unicode/ustdio.h>

#include "c-ohvu/io/stringref.h"
#include "c-ohvu/data/bdtrie.h"
#include "c-ohvu/data/sexpr.h"
#include "c-ohvu/runtime/builtins.h"
#include "c-ohvu/runtime/interpreter.h"

ovs_expr no_represent(void* d) { return ovs_list(0, NULL); }

void no_free(void* d) {}

/*
 * exit
 */

int32_t exit_apply(ovs_instruction* r, ovs_expr* args, void* d) {
	r->size = 0;
	return OVRU_SUCCESS;
}

ovs_function_info exit_inspect(void* d) {
	return (ovs_function_info){ 0, 0 };
}

static ovs_function_type exit_function = {
	u"scan",
	no_represent,
	exit_inspect,
	exit_apply,
	no_free
};

ovs_expr ovru_exit() {
	return ovs_function(&exit_function, 0, NULL);
}

/*
 * scanner
 */

typedef struct scanner_data {
	UFILE* file;
	ovs_expr file_name;
	ovs_expr* next;
	ovs_expr* text;
} scanner_data;

ovs_expr scanner_represent(void* d) {
	;
}

ovs_function_info scanner_inspect(void* d) {
	return (ovs_function_info){ 2, 2 };
}

int32_t scanner_apply(ovs_instruction* i, ovs_expr* args, void* d) {
	ovs_expr fail = args[0];
	ovs_expr cont = args[1];

	i->size = 1;
	i->values[0] = ovs_alias(fail);

	return OVRU_SUCCESS;
}

void scanner_free(void* d) {
	scanner_data data = *(scanner_data*)d;

	ovs_dealias(data.file_name);

	if (data.next) {
		ovs_dealias(*data.next);
		free(data.next);

		if (data.text) {
			free(data.text);
		}
	} else if (data.file) {
		u_fclose(data.file);
	}
}

static ovs_function_type scanner_function = {
	u"scan",
	scanner_represent,
	scanner_inspect,
	scanner_apply,
	scanner_free
};

ovs_expr ovru_open_scanner(UFILE* f, UChar* name) {
	scanner_data data = { f, ovs_string(ovio_u_strref(name)), NULL, NULL };
	return ovs_function(&scanner_function, sizeof(scanner_data), &data);
}

/*
 * printer
 */

typedef struct printer_data {
	UFILE* file;
	ovs_expr file_name;
	ovs_expr* next;
	ovs_expr* text;
} printer_data;

ovs_expr printer_represent(void* d) {
	;
}

ovs_function_info printer_inspect(void* d) {
	return (ovs_function_info){ 3, 1 };
}

int32_t printer_apply(ovs_instruction* i, ovs_expr* args, void* d) {
	ovs_expr string = args[0];
	ovs_expr fail = args[1];
	ovs_expr cont = args[2];
	
	printer_data data = *(printer_data*)d;

	i->size = 1;
	if (string.type != OVS_STRING) {
		i->values[0] = ovs_alias(fail);

	} else if (data.next == NULL) {
		u_fprintf(data.file, "%S", &string.p->string);
		i->values[0] = ovs_alias(cont);

	} else {
		

	}
	return OVRU_SUCCESS;
}

void printer_free(void* d) {
	printer_data data = *(printer_data*)d;

	ovs_dealias(data.file_name);

	if (data.next) {
		ovs_dealias(*data.next);
		free(data.next);

		if (data.text) {
			free(data.text);
		}
	} else if (data.file) {
		u_fclose(data.file);
	}
}

static ovs_function_type printer_function = {
	u"print",
	printer_represent,
	printer_inspect,
	printer_apply,
	printer_free
};

ovs_expr ovru_open_printer(UFILE* f, UChar* name) {
	printer_data data = { f, ovs_string(ovio_u_strref(name)), NULL, NULL };
	return ovs_function(&printer_function, sizeof(printer_data), &data);
}

/*
 * cons
 */

ovs_function_info cons_inspect(void* d) {
	return (ovs_function_info){ 3, 2 };
}

int32_t cons_apply(ovs_instruction* i, ovs_expr* args, void* d) {
	ovs_expr car = args[0];
	ovs_expr cdr = args[1];
	ovs_expr cont = args[2];

	i->size = 2;
	i->values[0] = ovs_alias(cont);
	i->values[1] = ovs_cons(car, cdr);

	return OVRU_SUCCESS;
}

static ovs_function_type cons_function = {
	u"cons",
	no_represent,
	cons_inspect,
	cons_apply,
	no_free
};

ovs_expr ovru_cons() {
	return ovs_function(&cons_function, 0, NULL);
}

/*
 * des
 */

ovs_function_info des_inspect(void* d) {
	return (ovs_function_info){ 3, 3 };
}

int32_t des_apply(ovs_instruction* i, ovs_expr* args, void* d) {
	ovs_expr e = args[0];
	ovs_expr fail = args[1];
	ovs_expr cont = args[2];

	if (ovs_atom(e)) {
		i->size = 1;
		i->values[0] = ovs_alias(fail);

	} else {
		i->size = 3;
		i->values[0] = ovs_alias(cont);
		i->values[1] = ovs_car(e);
		i->values[2] = ovs_cdr(e);
	}

	return OVRU_SUCCESS;
}

static ovs_function_type des_function = {
	u"des",
	no_represent,
	des_inspect,
	des_apply,
	no_free
};

ovs_expr ovru_des() {
	return ovs_function(&des_function, 0, NULL);
}

/*
 * eq
 */

ovs_function_info eq_inspect(void* d) {
	return (ovs_function_info){ 4, 1 };
}

int32_t eq_apply(ovs_instruction* i, ovs_expr* args, void* d) {
	ovs_expr e_a = args[0];
	ovs_expr e_b = args[1];
	ovs_expr f = args[2];
	ovs_expr t = args[3];

	i->size = 1;
	i->values[0] = ovs_alias(ovs_eq(e_a, e_b) ? t : f);

	return OVRU_SUCCESS;
}

static ovs_function_type eq_function = {
	u"eq",
	no_represent,
	eq_inspect,
	eq_apply,
	no_free
};

ovs_expr ovru_eq() {
	return ovs_function(&eq_function, 0, NULL);
}
