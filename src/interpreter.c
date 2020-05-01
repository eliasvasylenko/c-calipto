#include <stdbool.h>
#include <stdlib.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-calipto/stringref.h"
#include "c-calipto/sexpr.h"
#include "c-calipto/interpreter.h"

const UChar* data_namespace = u"data";
const UChar* function_namespace = u"function";
const UChar* system_namespace = u"system";

const UChar* lambda_name = u"lambda";
const UChar* quote_name = u"lambda";

s_bound_expr next_expression(s_bound_expr* tail) {
	if (s_atom(tail->form)) {
		*tail = (s_bound_expr){ NULL, NULL };
		return (s_bound_expr){ NULL, NULL };
	}

	tail = (s_bound_expr){ s_cdr(tail->form), tail->bindings };
	return (s_bound_expr){ s_car(tail->form), tail->bindings };
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

