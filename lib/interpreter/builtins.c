#include <stdbool.h>
#include <stdlib.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>
#include <unicode/ustdio.h>

#include "c-ohvu/idtrie.h"
#include "c-ohvu/stringref.h"
#include "c-ohvu/sexpr.h"

s_expr no_represent(void* d) { return s_list(0, NULL); }

void no_free(void* d) {}

/*
 * exit
 */

s_result exit_apply(s_instruction* r, s_expr* args, void* d) {
	r->size = 0;
	return S_SUCCESS;
}

s_function_info exit_inspect(void* d) {
	return (s_function_info){ 0, 0 };
}

static s_function_type exit_function = {
	u"scan",
	no_represent,
	exit_inspect,
	exit_apply,
	no_free
};

s_expr cal_exit() {
	return s_function(&exit_function, 0, NULL);
}

/*
 * scanner
 */

typedef struct scanner_data {
	UFILE* file;
	s_expr file_name;
	s_expr* next;
	s_expr* text;
} scanner_data;

s_expr scanner_represent(void* d) {
	;
}

s_function_info scanner_inspect(void* d) {
	return (s_function_info){ 2, 2 };
}

s_result scanner_apply(s_instruction* i, s_expr* args, void* d) {
	s_expr fail = args[0];
	s_expr cont = args[1];

	i->size = 1;
	i->values[0] = fail;

	return S_SUCCESS;
}

void scanner_free(void* d) {
	scanner_data data = *(scanner_data*)d;

	if (data.next) {
		s_dealias(*data.next);
		free(data.next);

		if (data.text) {
			free(data.text);
		}
	} else if (data.file) {
		u_fclose(data.file);
	}

	free(d);
}

static s_function_type scanner_function = {
	u"scan",
	scanner_represent,
	scanner_inspect,
	scanner_apply,
	scanner_free
};

s_expr cal_open_scanner(UFILE* f, s_expr name) {
	scanner_data data = { f, name, NULL, NULL };
	return s_function(&scanner_function, sizeof(scanner_data), &data);
}

/*
 * printer
 */

typedef struct printer_data {
	UFILE* file;
	s_expr file_name;
	s_expr* next;
	s_expr* text;
} printer_data;

s_expr printer_represent(void* d) {
	;
}

s_function_info printer_inspect(void* d) {
	return (s_function_info){ 3, 1 };
}

s_result printer_apply(s_instruction* i, s_expr* args, void* d) {
	s_expr string = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];
	
	printer_data data = *(printer_data*)d;

	i->size = 1;
	if (string.type != STRING) {
		i->values[0] = fail;

	} else if (data.next == NULL) {
		u_fprintf(data.file, "%S", &string.p->string);
		i->values[0] = cont;

	} else {
		

	}
	return S_SUCCESS;
}

void printer_free(void* d) {
	printer_data data = *(printer_data*)d;

	if (data.next) {
		s_dealias(*data.next);
		free(data.next);

		if (data.text) {
			free(data.text);
		}
	} else if (data.file) {
		u_fclose(data.file);
	}

	free(d);
}

static s_function_type printer_function = {
	u"print",
	printer_represent,
	printer_inspect,
	printer_apply,
	printer_free
};

s_expr cal_open_printer(UFILE* f, s_expr name) {
	printer_data data = { f, name, NULL, NULL };
	return s_function(&printer_function, sizeof(printer_data), &data);
}

/*
 * cons
 */

s_function_info cons_inspect(void* d) {
	return (s_function_info){ 3, 2 };
}

s_result cons_apply(s_instruction* i, s_expr* args, void* d) {
	s_expr car = args[0];
	s_expr cdr = args[1];
	s_expr cont = args[2];

	i->size = 2;
	i->values[0] = cont;
	i->values[1] = s_cons(car, cdr);

	return S_SUCCESS;
}

static s_function_type cons_function = {
	u"cons",
	no_represent,
	cons_inspect,
	cons_apply,
	no_free
};

s_expr cal_cons() {
	return s_function(&cons_function, 0, NULL);
}

/*
 * des
 */

s_function_info des_inspect(void* d) {
	return (s_function_info){ 3, 3 };
}

s_result des_apply(s_instruction* i, s_expr* args, void* d) {
	s_expr e = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];

	if (s_atom(e)) {
		i->size = 1;
		i->values[0] = fail;

	} else {
		i->size = 3;
		i->values[0] = cont;
		i->values[1] = s_car(e);
		i->values[2] = s_cdr(e);
	}

	return S_SUCCESS;
}

static s_function_type des_function = {
	u"des",
	no_represent,
	des_inspect,
	des_apply,
	no_free
};

s_expr cal_des() {
	return s_function(&des_function, 0, NULL);
}

/*
 * eq
 */

s_function_info eq_inspect(void* d) {
	return (s_function_info){ 4, 1 };
}

s_result eq_apply(s_instruction* i, s_expr* args, void* d) {
	s_expr e_a = args[0];
	s_expr e_b = args[1];
	s_expr f = args[2];
	s_expr t = args[3];

	i->size = 1;
	i->values[0] = s_eq(e_a, e_b) ? t : f;

	return S_SUCCESS;
}

static s_function_type eq_function = {
	u"eq",
	no_represent,
	eq_inspect,
	eq_apply,
	no_free
};

s_expr cal_eq() {
	return s_function(&eq_function, 0, NULL);
}
