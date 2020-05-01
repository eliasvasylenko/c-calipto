#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

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

s_expr eval_expression(s_bound_expr e) {
	if (s_atom(e.form)) {
		return s_resolve(e.form, e.bindings);
	}

	switch (e.form.type) {
	case LAMBDA:
		;
		if (e.form.lambda->free_var_count > 0) {
			/*
			 * TODO only capture free vars not entire scope
			 */
			s_ref_bindings(e.bindings);
			return s_function(e.bindings, e.form);
		} else {
			return s_function(s_alloc_bindings(NULL, 0, NULL), e.form);
		}

	case QUOTE:
		;
		s_expr data = *e.form.quote;
		s_ref(data);
		return data;

	default:
		return s_error(u_strref(u"Unable to resolve expression"));
	}
}

bool next_expression(s_expr* e, s_bound_expr* tail) {
	if (s_atom(tail->form)) {
		return false;
	}

	s_bound_expr head = { s_car(tail->form), tail->bindings };
	*e = eval_expression(head);
	*tail = (s_bound_expr){ s_cdr(tail->form), tail->bindings };
}

bool next_statement(s_bound_expr* s) {
	s_bound_expr tail = *s;

	s_expr target;
	if (!next_expression(&target, &tail)) {
		return false;
	}

	bool success = true;
	s_bound_expr result;
	switch (target.type) {
	case FUNCTION:
		;
		s_function_data f = *target.function;
		s_lambda_data l = *f.lambda.lambda;
		s_binding* bindings = malloc(sizeof(s_binding) * l.param_count);
		s_expr arg;
		for (int i = 0; i < l.param_count; i++) {
			next_expression(&arg, &tail);
			bindings[i] = (s_binding){ l.params[i], arg };
		}
		s_bindings* c;
		if (f.capture.count > 0) {
			c = malloc(sizeof(s_bindings));
			*c = f.capture;
			s_ref_bindings(*c);
		} else {
			c = NULL;
		}
		s_bindings b = s_alloc_bindings(c, l.param_count, bindings);

		*s = (s_bound_expr){ l.body, b };
		break;

	case BUILTIN:
		;
		s_builtin_data bi = *target.builtin;
		s_expr* args = malloc(sizeof(s_expr) * bi.arg_count);
		for (int i = 0; i < bi.arg_count; i++) {
			next_expression(&args[i], &tail);
		}
		success = bi.apply(args, s);
		break;

	default:
		break;
	}
	s_free(target);

	return success;
}

void eval(const s_expr e, const s_bindings b) {
	s_ref(e);
	s_bound_expr s = { e, b };
	do {
		if (s.form.type = ERROR) {
			printf("Error %s", s.form.error);
			return;
		}
	} while (next_statement(&s));
}

