#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-calipto/stringref.h"
#include "c-calipto/sexpr.h"
#include "c-calipto/interpreter.h"

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

static s_expr data_quote = { ERROR, NULL, .error=NULL };
static s_expr data_lambda = { ERROR, NULL, .error=NULL };

bool prepare_expression(s_expr* e) {
	if (s_atom(e)) {
		return true;
	}

	s_expr car = s_car(e);
	s_expr cdr = s_cdr(e);

	if (data_quote.type == ERROR) {
		data_quote = s_symbol(u_strref(u"data"), u_strref(u"quote"));
	}
	if (s_eq(car, data_quote)) {
		s_expr old = *e;
		*e = s_quote(cdr);
		s_free(old);
		return true;
	}

	if (data_lambda.type == ERROR) {
		data_lambda = s_symbol(u_strref(u"data"), u_strref(u"lambda"));
	}
	if (s_eq(car, data_lambda)) {

		/*
		 *
		 * TODO do all these based on s_atom, s_car etc not type == CONS
		 *
		 * TODO change params & vars to normal cons lists rather than arrays
		 *
		 */

		if (cdr.type != CONS ||
				cdr.cons->car.type != CONS ||
				cdr.cons->cdr.type != CONS ||
				cdr.cons->cdr.cons->car.type != CONS ||
				cdr.cons->cdr.cons->cdr.type != NIL) {
			return false;
		}
		s_expr params = cdr.cons->car;
		s_expr body = cdr.cons->cdr.cons->car;

		int32_t free_var_count = 0;
		int32_t param_count = 0;

		*e = s_lambda(free_var_count, NULL, param_count, NULL, body);
		s_free(old);
		return true;
	}
	
	return true;
}

bool next_expression(s_expr* e, s_bound_expr* tail) {
	if (s_atom(tail->form)) {
		return false;
	}

	s_bound_expr head = { s_car(tail->form), tail->bindings };
	/*
	 * TODO prepare the lambda or quote specialisation where appropriate
	 */
	prepare_expression(&head.form);
	*e = eval_expression(head);
	s_free(head.form);
	
	*tail = (s_bound_expr){ s_cdr(tail->form), tail->bindings };

	return true;
}

bool next_statement(s_bound_expr* s) {
	if (s->form.type == ERROR) {
		printf("Error: %s\n", s->form.error);
		return false;
	}

	s_bound_expr tail = *s;
	s_bound_expr previous = *s;

	s_expr target;
	if (!next_expression(&target, &tail)) {
		printf("Syntax error: ");
		s_dump(s->form);
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
		} else {
			c = NULL;
		}
		*s = (s_bound_expr){
			l.body,
			s_alloc_bindings(c, l.param_count, bindings)
	       	};
		break;

	case BUILTIN:
		;
		s_builtin_data bi = *target.builtin;
		s_expr* args = malloc(sizeof(s_expr) * bi.arg_count);
		for (int i = 0; i < bi.arg_count; i++) {
			next_expression(&args[i], &tail);
		}
		success = bi.apply(args, s);
		free(args);
		break;

	default:
		;
		*s = (s_bound_expr){
			s_error(u_strref(u"Invalid statement syntax")),
			s_alloc_bindings(NULL, 0, NULL)
		};
		break;
	}
	if (success) {
		s_free(previous.form);
		s_free_bindings(previous.bindings);
	}

	return success;
}

void eval(const s_expr e, const s_bindings b) {
	s_bound_expr s = { e, b };
	s_ref(s.form);
	s_ref_bindings(s.bindings);

	do {
		printf("  trace: ");
		s_dump(s.form);
	} while (next_statement(&s));

	s_free(s.form);
	s_free_bindings(s.bindings);
}

