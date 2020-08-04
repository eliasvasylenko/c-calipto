#include "c-ohvu/data/sexpr.h"
#include "c-ohvu/runtime/evaluator.h"
#include "c-ohvu/runtime/compiler.h"
#include "compilerapi.h"

typedef struct statement_data {
	compile_state* state;
} statement_data;

ovs_expr statement_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

void statement_free(const void* d) {
	statement_data* data = *(statement_data**)d;

	free_compile_state(data->state);
	free(data);
}

ovs_function_info statement_inspect(const ovs_function_data* d);

int32_t statement_with_lambda_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t statement_with_variable_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t statement_with_quote_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t statement_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);

static ovs_function_type statement_with_lambda_function = {
	u"statement-with-lambda",
	statement_represent,
	statement_inspect,
	statement_with_lambda_apply,
	statement_free
};

static ovs_function_type statement_with_variable_function = {
	u"statement-with-variable",
	statement_represent,
	statement_inspect,
	statement_with_variable_apply,
	statement_free
};

static ovs_function_type statement_with_quote_function = {
	u"statement-with-quote",
	statement_represent,
	statement_inspect,
	statement_with_quote_apply,
	statement_free
};

static ovs_function_type statement_end_function = {
	u"statement-end",
	statement_represent,
	statement_inspect,
	statement_end_apply,
	statement_free
};

ovs_function_info statement_inspect(const ovs_function_data* d) {
	if (d->type == &statement_with_lambda_function)
		return (ovs_function_info){ 3, 2 };

	else if (d->type == &statement_with_lambda_function)
		return (ovs_function_info){ 2, 2 };

	else if (d->type == &statement_with_quote_function)
		return (ovs_function_info){ 2, 2 };

	else if (d->type == &statement_end_function)
		return (ovs_function_info){ 1, 2 };

	assert(false);
}

ovs_expr statement_function(compile_state* s, ovs_function_type* t) {
	statement_data* d;
	ovs_expr e = ovs_function(s->context, t, sizeof(statement_data*), (void**)&d);
	d->state = ref_compile_state(s);
	return e;
}

compile_state* statement_with(ovs_instruction* i, statement_data* e, ovs_expr cont, ovru_term t) {
	compile_state* s = e->state;
	if (!is_unique_compile_state(s)) {
		s = make_compile_state(s, s->context, s->cont);
		without_parameters(s);
	}

	with_term(s, t);

	i->size = 5;
	i->values[0] = ovs_alias(cont);
	i->values[1] = statement_function(s, &statement_with_lambda_function);
	i->values[2] = statement_function(s, &statement_with_variable_function);
	i->values[3] = statement_function(s, &statement_with_quote_function);
	i->values[4] = statement_function(s, &statement_end_function);

	return s;
}

/**
 * Flatten a stack of compile_states down to the nearest enclosing lambda.
 *
 * Flattening here means concatenating all the partial lists of captures and terms
 *
 * This is deferred until the enclosing lambda is completed for two reasons:
 * - to avoid redundant allocation and copying when concatenating capture and term lists,
 * - so that we don't need to populate the lookup table for parameters and captures.
 */
compile_state* flatten_compile_state(compile_state* s, int32_t term_count, int32_t capture_count) {
	term_count += s->body.term_count;
	capture_count += s->capture_count;

	compile_state* f;

	if (s->param_count >= 0) {
		f = make_compile_state(s->parent, s->context, s->cont);
		ref_compile_state(f);

		f->param_count = s->param_count;
		f->params = ovs_alias(s->params);

		f->capture_count = capture_count;
		f->total_capture_count = capture_count;
		if (f->capture_count > 0)
			f->captures = malloc(sizeof(ovru_variable) * f->capture_count);

		f->body.term_count = term_count;
		if (f->body.term_count > 0)
			f->body.terms = malloc(sizeof(ovru_term) * f->body.term_count);


	} else {
		f = flatten_compile_state(s->parent, term_count, capture_count);
	}

	variable_capture* captures = f->captures + f->capture_count - capture_count;
	for (int i = 0; i < s->capture_count; i++) {
		f->captures[i] = s->captures[i];
	}

	ovru_term* terms = s->body.terms + f->body.term_count - term_count;
	for (int i = 0; i < s->body.term_count; i++) {
		f->body.terms[i] = s->body.terms[i];
	}

	free_compile_state(s);

	return f;
}

int32_t statement_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* data = ovs_function_extra_data(f);
	compile_state* s = data->state;
	compile_state* p = s->parent;
	ovs_expr cont = { OVS_FUNCTION, .p=s->cont };

	if (s->param_count < 0) {
		s = flatten_compile_state(s, 0, 0);
	}

	ovru_lambda* l = malloc(sizeof(ovru_lambda));
	ovru_term t = (ovru_term){ .type=OVRU_LAMBDA, .lambda=l };

	l->ref_count = ATOMIC_VAR_INIT(1);
	l->param_count = s->param_count;
	l->params = ovs_alias(s->params);

	l->body.term_count = s->body.term_count;
	if (l->body.term_count > 0) {
		l->body.terms = malloc(sizeof(ovru_term) * l->body.term_count);
		for (int i = 0; i < l->body.term_count; i++) {
			l->body.terms[i] = ovru_alias_term(s->body.terms[i]);
		}
	}

	if (p == NULL) {
		assert(s->capture_count == 0);
		l->capture_count = 0;

		bound_lambda_data* l;
		ovs_expr f = ovs_function(s->context, &bound_lambda_function, sizeof(bound_lambda_data), (void**)&l);
		*l = (bound_lambda_data){ ref_lambda(t.lambda), NULL };

		i->size = 2;
		i->values[0] = ovs_alias(cont);
		i->values[1] = f;

	} else {
		uint32_t propagated_count = 0;
		variable_capture* propagated_captures;
		l->capture_count = s->capture_count;
		if (s->capture_count > 0) {
			l->captures = malloc(sizeof(ovru_variable) * l->capture_count);

			if (s->propagated_capture_count > 0) {
				propagated_captures = malloc(sizeof(variable_capture) * s->propagated_capture_count);
			}

			for (int i = 0; i < s->capture_count; i++) {
				if (s->captures[i].depth > 1) {
					variable_capture* propagated = &propagated_captures[propagated_count];
					*propagated = s->captures[i];
					propagated->depth--;
					l->captures[i] = (ovru_variable){ OVRU_CAPTURE, propagated_count + p->total_capture_count };
					propagated_count++;
				} else {
					l->captures[i] = s->captures[i].variable;
				}
			}

			assert(propagated_count == s->propagated_capture_count);
		}

		s = statement_with(i, data, cont, t);

		with_captures(s, propagated_count, propagated_captures);
	}

	return 0;
}

int32_t statement_with_lambda_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* data = ovs_function_extra_data(f);

	ovs_expr params = args[1];
	ovs_expr body = args[2];
	ovs_expr cont = args[3];

	ovs_expr lambda_params = ovs_root_symbol(OVS_DATA_NIL)->expr;
	compile_state* s = make_compile_state(data->state, data->state->context, cont.p);

	i->size = 3;
	i->values[0] = ovs_alias(params);
	i->values[1] = parameters_function(s, lambda_params, body.p, &parameters_with_function);
	i->values[2] = parameters_function(s, lambda_params, body.p, &parameters_end_function);

	ovs_dealias(lambda_params);
}

int32_t statement_with_variable_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* e = ovs_function_extra_data(f);

	ovs_expr receiver = args[0];
	ovs_expr variable = args[1];
	ovs_expr cont = args[2];

	ovru_variable v;
	uint32_t depth = variable.type = OVS_SYMBOL ? find_variable(&v, e->state, variable.p) : -1;

	if (depth < 0) {
		// TODO how to deal with errors???
	}

	ovru_term t;
	t.type = OVRU_VARIABLE;
	if (depth == 0) {
		t.variable = v;
		compile_state* s = statement_with(i, e, cont, t);

	} else {
		t.variable = (ovru_variable){ OVRU_CAPTURE, e->state->total_capture_count };
		compile_state* s = statement_with(i, e, cont, t);

		variable_capture c[] = { { variable.p, v, depth } };
		with_captures(s, 1, c);
	}

	return 0;
}

int32_t statement_with_quote_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* e = ovs_function_extra_data(f);

	ovs_expr receiver = args[0];
	ovs_expr data = args[1];
	ovs_expr cont = args[2];

	ovru_term t = { .quote=data };

	statement_with(i, e, cont, t);
	return 0;
}

