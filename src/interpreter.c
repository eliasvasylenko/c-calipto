#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-calipto/stringref.h"
#include "c-calipto/idtrie.h"
#include "c-calipto/sexpr.h"
#include "c-calipto/interpreter.h"

bool compile_quote(s_term* result, int32_t part_count, s_expr* parts) {
	if (part_count != 1) {
		return false;
	}

	*result = (s_term){ .quote=parts[0] };

	return true;
}

bool compile_statement(s_expr* result, s_expr s);

s_term s_alias_term(s_term t) {
	switch (t.type) {
		case LAMBDA:
			s_ref_lambda(t.lambda);
			break;
		case VARIABLE:
			break;
		default:
			s_alias(t.quote);
			break;
	}
	return t;
}

void s_dealias_term(s_term t) {
	switch (t.type) {
		case LAMBDA:
			s_free_lambda(t.lambda);
			break;
		case VARIABLE:
			break;
		default:
			s_dealias(t.quote);
			break;
	}
}

s_lambda* s_ref_lambda(s_lambda* l) {
	atomic_fetch_add(&l->ref_count, 1);
}

void s_free_lambda(s_lambda* l) {
	if (atomic_fetch_add(&l->ref_count, -1) > 1) {
		if (l->param_count > 0) {
			for (int i = 0; i < l->param_count; i++) {
				s_free(SYMBOL, l->params[i]);
			}
			free(l->params);
		}
		if (l->var_count > 0) {
			free(l->vars);
			for (int i = 0; i < l->term_count; i++) {
				s_dealias_term(l->terms[i]);
			}
		}
		if (l->term_count > 0) {
			free(l->terms);
		}
	}
}

bool compile_lambda(s_term* result, int32_t part_count, s_expr* parts) {
	if (part_count != 2) {
		return false;
	}

	s_expr params_decl = parts[0];
	s_expr body_decl = parts[1];

	s_expr_ref** params;
	int32_t param_count = s_delist(params_decl, &params);
	if (param_count < 0) {
		return false;
	}

	s_expr body;
	bool success = compile_statement(&body, body_decl);

	if (success) {
		s_lambda* l = malloc(sizeof(s_lambda));
		l->ref_count = ATOMIC_VAR_INIT(1);
		l->param_count = param_count;
		if (param_count > 0) {
			l->params = malloc(sizeof(s_expr_ref*) * param_count);
			for (int i = 0; i < param_count; i++) {
				s_ref(params[i]);
				l->params[i] = params[i];
			}
		} else {
			l->params = NULL;
		}
		l->var_count = var_count;
		if (var_count > 0) {
			l->vars = malloc(sizeof(uint32_t) * var_count);
			for (int i = 0; i < var_count; i++) {
				l->vars[i] = vars[i];
			}
		} else {
			l->vars = NULL;
		}
		l->term_count = term_count;
		if (term_count > 0) {
			l->terms = malloc(sizeof(s_term) * term_count);
			for (int i = 0; i < term_count; i++) {
				s_alias_term(terms[i]);
				l->terms[i] = terms[i];
			}
		} else {
			l->terms = NULL;
		}
		*result = (s_term){ .type=LAMBDA, .lambda=l };
			*result = s_lambda(param_count, params, body);
			s_free(body);
	}

	if (param_count > 0) {
		for (int i = 0; i < param_count; i++) {
			s_free(params[i]);
		}
		free(params);
	}

	return success;
}

bool compile_expression(s_term* result, s_expr e) {
	if (s_atom(e)) {
		s_ref(e);
		*result = e;
		return true;
	}

	int32_t count;
	s_expr* parts;
	if (!flatten_list(e, &count, &parts) || count == 0) {
		printf("Syntax error in expression: ");
		s_dump(e);
		return false;
	}

	s_expr kind = parts[0];
	int32_t term_count = count - 1;
	s_expr* terms = parts + 1;

	if (data_quote.type == ERROR) {
		data_quote = s_symbol(u_strref(u"data"), u_strref(u"quote"));
	}
	if (data_lambda.type == ERROR) {
		data_lambda = s_symbol(u_strref(u"data"), u_strref(u"lambda"));
	}

	bool success;
	if (s_eq(kind, data_lambda)) {
		success = compile_lambda(result, term_count, terms);

	} else if (s_eq(kind, data_quote)) {
		success = compile_quote(result, term_count, terms);

	} else {
		success = false;
	}

	for (int i = 0; i < count; i++) {
		s_free(parts[i]);
	}
	free(parts);
	
	return success;
}

bool compile_statement(s_statement* result, s_expr s) {
	if (s.type == STATEMENT) {
		s_ref(s);
		*result = s;
		return true;
	}

	int32_t count;
	s_expr* expressions;
	if (!flatten_list(s, &count, &expressions) || count == 0) {
		printf("Syntax error in statement: ");
		s_dump(s);
		return false;
	}

	bool success = true;
	for (int i = 0; i < count; i++) {
		s_expr e;
		if (compile_expression(&e, expressions[i])) {
			s_free(expressions[i]);
			expressions[i] = e;
		} else {
			success = false;
			break;
		}
	}

	if (success) {
		int32_t free_var_count = 0;
		s_expr* free_vars = NULL;

		*result = s_statement(free_var_count, free_vars,
				expressions[0],
				count - 1, expressions + 1);
	}

	for (int i = 0; i < count; i++) {
		s_free(expressions[i]);
	}
	free(expressions);

	return success;
}

bool eval_expression(s_expr* result, s_bound_expr e) {
	if (s_atom(e.form)) {
		return s_resolve(result, e.form, e.bindings);
	}

	switch (e.form.type) {
		case LAMBDA:
			;
			if (e.form.lambda->body.statement->free_var_count > 0 || 1) {
				/*
				 * TODO only capture free vars not entire scope
				 */
				*result = s_function(e.bindings, e.form);
				return true;
			} else {
				s_bindings b = s_alloc_bindings(NULL, 0, NULL);
				*result = s_function(b, e.form);
				s_free_bindings(b);
				return true;
			}

		case QUOTE:
			;
			s_expr data = *e.form.quote;
			s_ref(data);
			*result = data;
			return true;

		default:
			return false;
	}
}

bool eval_function(s_bound_expr* result, s_expr target, int32_t arg_count, s_expr* args) {
	s_lambda_data l = *target.function->lambda.lambda;
	if (arg_count != l.param_count) {
		return false;
	}
	
	s_bindings bindings;
	if (arg_count == 0) {
		bindings = target.function->capture;
		s_ref_bindings(bindings);
	} else {
		s_binding* b = malloc(sizeof(s_binding) * arg_count);
		for (int i = 0; i < arg_count; i++) {
			b[i] = (s_binding){ l.params[i], args[i] };
		}
		bindings = s_alloc_bindings(&target.function->capture, arg_count, b);
		free(b);
	}
	s_ref(l.body);
	*result = (s_bound_expr){
		l.body,
		bindings	
	};

	return true;
}

bool eval_statement(s_bound_expr* result, s_bound_expr s) {
	// printf("  trace: ");
	// s_dump(s.form);

	if (!s.form.type == STATEMENT) {
		printf("Unable to resolve statement: ");
		s_dump(s.form);
		return false;
	}

	s_bound_expr bound;
	bound.bindings = s.bindings;
	
	bound.form = s.form.statement->target;
	s_expr target;
	if (!eval_expression(&target, bound)) {
		printf("Failed to evaluate target: ");
		s_dump(bound.form);
		return false;
	}
	int32_t arg_count = s.form.statement->arg_count;
	s_expr* args = malloc(sizeof(s_expr) * arg_count);
	for (int i = 0; i < arg_count; i++) {
		bound.form = s.form.statement->args[i];
		if (!eval_expression(args + i, bound)) {
			printf("Failed to evaluate argument: ");
			s_dump(bound.form);
			s_free(target);
			for (int j = 0; j < i; j++) {
				s_free(args[j]);
			}
			free(args);
			return false;
		}
	}

	bool success;
	if (target.type == FUNCTION) {
		if (arg_count != target.builtin->arg_count) {
			return false;
		}

		success = target.builtin->apply(result, args, target.builtin->data);

	} else {
		printf("Unable to apply to target: ");
		s_dump(target);
		success = false;
		break;
	}

	s_free(target);
	for (int i = 0; i < arg_count; i++) {
		s_free(args[i]);
	}
	free(args);

	return success;
}

static s_expr s_data;
static s_expr s_data_nil;

s_statement s_compile(const s_expr e, const uint32_t param_count, const s_expr_ref** params) {
	;
}

void s_eval(const s_statement s, const s_expr* args) {
	s_data = s_symbol(NULL, u_strref(u"data"));
	s_data_nil = s_symbol(s_data.p, u_strref(u"nil"));

	s_expr c;
	s_bound_expr next;
	while (compile_statement(&c, s.form)) {
		s_free(s.form);
		s.form = c;
		if (eval_statement(&next, s)) {
			s_free(s.form);
			s_free_bindings(s.bindings);
			s = next;
		} else {
			break;
		}
	}

	s_dealias(s_data);
	s_dealias(s_data_nil);
}

