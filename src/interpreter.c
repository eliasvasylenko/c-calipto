#include <stdbool.h>
#include <stdlib.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-calipto/sexpr.h"
#include "c-calipto/interpreter.h"

const UChar* data_namespace = u"data";
const UChar* function_namespace = u"function";
const UChar* system_namespace = u"system";

const UChar* lambda_name = u"lambda";
const UChar* quote_name = u"lambda";

bindings* make_bindings(const bindings* p, int32_t c, const binding* b) {
	bindings* bs = malloc(sizeof(bindings) + sizeof(binding) * c);
	bs->parent = (bindings*)p;
	bs->count = c;
	binding* a = (binding*)(bs + 1);
	for (int i = 0; i < c; i++) {
		a[i] = b[i];
	}
	return bs;
}

void free_bindings(bindings* p) {

}

sexpr* resolve(const bindings* b, const sexpr* name) {
	if (b == NULL) {
		return NULL;
	}

	binding* a = (binding*)(b + 1);

	for (int i = 0; i < b->count; i++) {
		binding b = a[i];
		if (sexpr_eq(name, b.name)) {
			return b.value;
		}
	}

	return resolve(b->parent, name);
}

sexpr* eval_expression(expression e) {
	if (sexpr_atom(e.form)) {
		return resolve(e.bindings, e.form);
	}

	switch (e.form->type) {
	case LAMBDA:;
		lambda l = *(lambda*)(e.form + 1);
		sexpr** capture = malloc(sizeof(sexpr*) * l.free_var_count);
		for (int i = 0; i < l.free_var_count; i++) {
			capture[i] = resolve(e.bindings, l.free_vars[i]);
		}
		
		return sexpr_function(l, capture);

	case QUOTE:
		return *(sexpr**)(e.form + 1);

	default:
		return NULL;
	}
}

expression next_expression(statement* tail) {
	if (sexpr_atom(tail->form)) {
		*tail = (statement){ NULL, NULL };
		return (expression){ NULL, NULL };
	}

	*tail = (statement){ sexpr_cdr_borrow(tail->form), tail->bindings };
	return (expression){ sexpr_car_borrow(tail->form), tail->bindings };
}

statement eval_statement(const statement s) {
	statement tail = s;
	sexpr* target = eval_expression(next_expression(&tail));

	statement result;
	switch (target->type) {
	case FUNCTION:
		;
		function f = *(function*)(target + 1);
		binding* bindings = malloc(sizeof(binding) * f.lambda.param_count);
		for (int i = 0; i < f.lambda.param_count; i++) {
			sexpr* arg = eval_expression(next_expression(&tail));
			bindings[i] = (binding){ f.lambda.params[i], arg };
		}

		result = NULL;

	case BUILTIN:
		;
		builtin b = *(builtin*)(target + 1);
		sexpr** args = malloc(sizeof(sexpr*) * b.arg_count);
		for (int i = 0; i < b.arg_count; i++) {
			args[i] = eval_expression(next_expression(&tail));
		}
		result = b.apply(args);

	default:
		result = (statement){ NULL, NULL };
	}
	sexpr_free(target);

	return result;
}

void eval(const sexpr* e, const bindings* b) {
	statement s = { b, e };
	do {
		s = eval_statement(s);
	} while (s.form != NULL);
}

