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

bool make_quote(s_expr* e, s_expr s, s_expr data) {
	if (data_quote.type == ERROR) {
		data_quote = s_symbol(u_strref(u"data"), u_strref(u"quote"));
	}
	if (!s_eq(s, data_quote) || s_atom(data)) {
		return false;
	}

	s_expr terminal = s_cdr(data);
	if (!s_eq(terminal, s_nil())) {
		s_free(terminal);
		return false;
	}
	s_free(terminal);

	data = s_car(data);

	s_expr old = *e;
	*e = s_quote(data);
	s_free(old);

	s_free(data);

	return true;
}

bool make_lambda(s_expr* e, s_expr s, s_expr lambda) {
	if (data_lambda.type == ERROR) {
		data_lambda = s_symbol(u_strref(u"data"), u_strref(u"lambda"));
	}
	if (!s_eq(s, data_lambda) || s_atom(lambda)) {
		return false;
	}
	
	s_expr tail = s_cdr(lambda);
	if (s_atom(tail)) {
		s_free(tail);
		return false;
	}

	s_expr terminal = s_cdr(tail);
	if (!s_eq(terminal, s_nil())) {
		s_free(tail);
		s_free(terminal);
		return false;
	}
	s_free(terminal);

	s_expr params = s_car(lambda);
	s_expr body = s_car(tail);
	s_free(tail);

	int32_t free_var_count = 0;
	int32_t param_count = 0;

	s_expr old = *e;
	*e = s_lambda(free_var_count, NULL, param_count, NULL, body);
	s_free(old);

	s_free(params);
	s_free(body);
	
	return true;
}

bool prepare_expression(s_expr* e) {
	if (s_atom(*e)) {
		return true;
	}

	s_expr car = s_car(*e);
	s_expr cdr = s_cdr(*e);

	bool success = make_quote(e, car, cdr) || make_lambda(e, car, cdr);

	s_free(car);
	s_free(cdr);
	
	return success;
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
	
	s_expr last_tail = tail->form;
	*tail = (s_bound_expr){ s_cdr(tail->form), tail->bindings };
	s_free(last_tail);

	return true;
}

bool flatten_list(s_expr s, int32_t* count, s_expr** expressions) {

}

bool next_statement(s_bound_expr s, s_bound_expr* result) {
	printf("  trace: ");
	s_dump(s.form);

	if (s.form.type == ERROR) {
		printf("Error: %s\n", s.form.error);
		return false;
	}

	int32_t count;
	s_expr* expressions;
	if (!flatten_list(s.form, &count, &expressions) || count == 0) {
		printf("Syntax error: ");
		s_dump(s.form);
		return false;
	}

	s_expr target = expressions[0];
	int32_t arg_count = count - 1;
	s_expr* args = expressions + 1;

	bool success;
	switch (target.type) {
	case FUNCTION:
		;
		s_lambda_data l = *target.function->lambda.lambda;
		if (arg_count != l.param_count) {
			success = false;
		} else {
			success = true;

			s_binding* bindings = malloc(sizeof(s_binding) * arg_count);
			for (int i = 0; i < arg_count; i++) {
				bindings[i] = (s_binding){ l.params[i], args[i] };
			}
			s_bindings* capture = &target.function->capture;
			*result = (s_bound_expr){
				l.body,
				s_alloc_bindings(capture, arg_count, bindings)
			};
			free(bindings);
		}
		break;

	case BUILTIN:
		;
		if (arg_count != target.builtin->arg_count) {
			success = false;
		} else {
			success = target.builtin->apply(args, result);
		}
		break;

	default:
		;
		success = true;
		*result = (s_bound_expr){
			s_error(u_strref(u"Invalid statement syntax")),
			s_alloc_bindings(NULL, 0, NULL)
		};
		break;
	}

	for (int i = 0; i < count; i++) {
		s_free(expressions[i]);
	}
	free(expressions);

	return success;
}

void eval(const s_expr e, const s_bindings b) {
	s_bound_expr s = { e, b };

	s_ref(s.form);
	s_ref_bindings(s.bindings);

	s_bound_expr next;
	while (next_statement(s, &next)) {
		s_free(s.form);
		s_free_bindings(s.bindings);
		s = next;
	}

	s_free(s.form);
	s_free_bindings(s.bindings);
}

