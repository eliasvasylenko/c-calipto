#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>
#include <unicode/ustdio.h>

#include "c-ohvu/io/stringref.h"
#include "c-ohvu/data/bdtrie.h"
#include "c-ohvu/data/sexpr.h"
#include "c-ohvu/runtime/builtins.h"
#include "c-ohvu/runtime/interpreter.h"

ovs_expr no_represent(ovs_context* c, UChar* name, const void* d) {
	return ovs_symbol(c->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(name), name);
}

void no_free(const void* d) {}

/*
 * exit
 */

int32_t exit_apply(ovs_instruction* r, ovs_expr* args, const void* d) {
	r->size = 0;
	return OVRU_SUCCESS;
}

ovs_function_info exit_inspect(const void* d) {
	return (ovs_function_info){ 0, 0 };
}

static ovs_function_type exit_function = {
	u"exit",
	no_represent,
	exit_inspect,
	exit_apply,
	no_free
};

ovs_expr ovru_exit(ovs_context* c) {
	return ovs_function(c, &exit_function, 0, NULL);
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

ovs_expr scanner_represent(ovs_context* c, UChar* name, const void* d) {
	assert(false);
}

ovs_function_info scanner_inspect(const void* d) {
	return (ovs_function_info){ 2, 2 };
}

int32_t scanner_apply(ovs_instruction* i, ovs_expr* args, const void* d) {
	ovs_expr fail = args[0];
	ovs_expr cont = args[1];

	i->size = 1;
	i->values[0] = ovs_alias(fail);

	return OVRU_SUCCESS;
}

void scanner_free(const void* d) {
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

ovs_expr ovru_open_scanner(ovs_context* c, UFILE* f, UChar* name) {
	scanner_data data = { f, ovs_string(u_strlen(name), name), NULL, NULL };
	return ovs_function(c, &scanner_function, sizeof(scanner_data), &data);
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

ovs_expr printer_represent(ovs_context*c, UChar* name, const void* d) {
	assert(false);
}

ovs_function_info printer_inspect(const void* d) {
	return (ovs_function_info){ 3, 1 };
}

int32_t printer_apply(ovs_instruction* i, ovs_expr* args, const void* d) {
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

void printer_free(const void* d) {
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

ovs_expr ovru_open_printer(ovs_context* c, UFILE* f, UChar* name) {
	printer_data data = { f, ovs_string(u_strlen(name), name), NULL, NULL };
	return ovs_function(c, &printer_function, sizeof(printer_data), &data);
}

/*
 * cons
 */

ovs_expr cons_represent(ovs_context*c, UChar* name, const void* d) {
	assert(false);
}

ovs_function_info cons_inspect(const void* d) {
	return (ovs_function_info){ 3, 2 };
}

int32_t cons_apply(ovs_instruction* i, ovs_expr* args, const void* d) {
	ovs_expr car = args[0];
	ovs_expr cdr = args[1];
	ovs_expr cont = args[2];

	i->size = 2;
	i->values[0] = ovs_alias(cont);
	i->values[1] = ovs_cons((ovs_table*)d, car, cdr);

	return OVRU_SUCCESS;
}

void cons_free(const void* d) {
	ovs_table* t = (ovs_table*)d;
	if (t->qualifier != NULL) {
		ovs_free(OVS_SYMBOL, t->qualifier);
	}
}

static ovs_function_type cons_function = {
	u"cons",
	cons_represent,
	cons_inspect,
	cons_apply,
	cons_free	
};

ovs_expr ovru_cons(ovs_context* c, ovs_table* t) {
	if (t->qualifier != NULL) {
		ovs_ref(t->qualifier);
	}
	return ovs_function(c, &cons_function, 0, t);
}

/*
 * des
 */

ovs_expr des_represent(ovs_context* c, UChar* name, const void* d) {
	assert(false);
}

ovs_function_info des_inspect(const void* d) {
	return (ovs_function_info){ 3, 3 };
}

int32_t des_apply(ovs_instruction* i, ovs_expr* args, const void* d) {
	ovs_expr e = args[0];
	ovs_expr fail = args[1];
	ovs_expr cont = args[2];

	if (ovs_is_atom((ovs_table*)d, e)) {
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

void des_free(const void* d) {
	ovs_table* t = (ovs_table*)d;
	if (t->qualifier != NULL) {
		ovs_free(OVS_SYMBOL, t->qualifier);
	}
}

static ovs_function_type des_function = {
	u"des",
	des_represent,
	des_inspect,
	des_apply,
	des_free
};

ovs_expr ovru_des(ovs_context* c, ovs_table* t) {
	if (t->qualifier != NULL) {
		ovs_ref(t->qualifier);
	}
	return ovs_function(c, &des_function, 0, t);
}

/*
 * eq
 */

ovs_function_info eq_inspect(const void* d) {
	return (ovs_function_info){ 4, 1 };
}

int32_t eq_apply(ovs_instruction* i, ovs_expr* args, const void* d) {
	ovs_expr e_a = args[0];
	ovs_expr e_b = args[1];
	ovs_expr f = args[2];
	ovs_expr t = args[3];

	i->size = 1;
	i->values[0] = ovs_alias(ovs_is_eq(e_a, e_b) ? t : f);

	return OVRU_SUCCESS;
}

static ovs_function_type eq_function = {
	u"eq",
	no_represent,
	eq_inspect,
	eq_apply,
	no_free
};

ovs_expr ovru_eq(ovs_context* c) {
	return ovs_function(c, &eq_function, 0, NULL);
}
