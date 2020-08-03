#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-ohvu/io/stringref.h"
#include "c-ohvu/data/bdtrie.h"
#include "c-ohvu/data/sexpr.h"
#include "c-ohvu/runtime/compiler.h"
#include "lambda.h"
#include "compilerapi.h"

void* get_variable_binding(uint32_t key_size, const void* key_data, const void* value_data, bdtrie_node* owner) {
	ovru_variable* value = malloc(sizeof(ovru_variable*));
	*value = *((ovru_variable*)value_data);
	return value;
}

void update_variable_binding(void* value, bdtrie_node* owner) {}

void free_variable_binding(void* value) {
	free(value);
}

compile_state* make_compile_state(compile_state* parent, ovs_context* oc, const ovs_expr_ref* cont) {
	compile_state* s = malloc(sizeof(compile_state));
	s->counter = ATOMIC_VAR_INIT(0);

	s->parent = ref_compile_state(parent);
	s->context = oc;

	s->variables = (bdtrie){
		NULL,
		get_variable_binding,
		update_variable_binding,
		free_variable_binding
	};
	s->capture_count = 0;
	s->propagated_capture_count = 0;

	s->body.term_count = 0;

	s->cont = ovs_ref(cont);

	return s;
}

void without_parameters(compile_state* s) {
	s->total_capture_count = s->parent->total_capture_count;
	s->propagated_capture_count = s->parent->propagated_capture_count;

	s->param_count = -1;
}

void with_parameters(compile_state* s, ovs_expr params) {
	s->total_capture_count = 0;
	s->propagated_capture_count = 0;

	s->param_count = 0;
	s->params = ovs_alias(params);
	while (!ovs_is_symbol(params)) {
		ovs_expr head = ovs_car(params);
		ovs_expr tail = ovs_cdr(params);
		ovs_dealias(params);
		params = tail;

		ovru_variable v = { OVRU_PARAMETER, s->param_count };
		bdtrie_insert(&s->variables, sizeof(ovs_expr_ref*), head.p, &v);
		ovs_dealias(head);

		s->param_count++;
	}
	ovs_dealias(params);
}

void with_captures(compile_state* s, uint32_t capture_count, variable_capture* captures) {
	uint32_t previous_capture_count = s->capture_count;
	variable_capture* previous_captures = s->captures;

	s->total_capture_count += capture_count;
	s->capture_count += capture_count;
	s->captures = malloc(sizeof(ovru_term) * s->capture_count);
	if (previous_capture_count > 0) {
		for (int i = 0; i < previous_capture_count; i++) {
			s->captures[i] = captures[i];
		}
		free(captures);
	}
	for (int i = 0; i < capture_count; i++) {
		s->captures[i + previous_capture_count] = captures[i];
		if (captures[i].depth == 1) {
			s->propagated_capture_count++;
		}
	}
}

void with_term(compile_state* s, ovru_term t) {
	ovru_statement b = s->body;

	s->body.term_count++;
	s->body.terms = malloc(sizeof(ovru_term) * s->body.term_count);
	if (b.term_count > 0) {
		for (int i = 0; i < b.term_count; i++) {
			s->body.terms[i] = b.terms[i];
		}
		free(b.terms);
	}
	s->body.terms[b.term_count] = ovru_alias_term(t);
}

/*
 * API
 */

ovs_expr statement_function(compile_state* s, ovs_function_type* t) {
	statement_data* d;
	ovs_expr e = ovs_function(s->context, t, sizeof(statement_data*), (void**)&d);
	d->state = ref_compile_state(s);
	return e;
}

ovs_expr parameters_function(compile_state* s, ovs_expr params, const ovs_expr_ref* cont, ovs_function_type* t) {
	parameters_data* p;
	ovs_expr e = ovs_function(s->context, t, sizeof(parameters_data*), (void**)&p);
	p->state = ref_compile_state(s);
	p->params = ovs_alias(params);
	p->cont = ovs_ref(cont);
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

		ovs_expr f = bind_lambda(l, NULL);

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

	return OVRU_SUCCESS;
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

	return OVRU_SUCCESS;
}

int32_t statement_with_quote_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* e = ovs_function_extra_data(f);

	ovs_expr receiver = args[0];
	ovs_expr data = args[1];
	ovs_expr cont = args[2];

	ovru_term t = { .quote=data };

	statement_with(i, e, cont, t);
	return OVRU_SUCCESS;
}

int32_t parameters_with_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	parameters_data* e = ovs_function_extra_data(f);

	ovs_expr param = args[1];
	ovs_expr cont = args[2];

	ovs_expr params = ovs_cons(&f->context->root_tables[OVS_UNQUALIFIED], param, e->params);

	i->size = 3;
	i->values[0] = ovs_alias(cont);
	i->values[1] = parameters_function(e->state, params, e->cont, &parameters_with_function);
	i->values[2] = parameters_function(e->state, params, e->cont, &parameters_end_function);

	ovs_dealias(params);

	return OVRU_SUCCESS;
}

int32_t parameters_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	parameters_data* e = ovs_function_extra_data(f);

	ovs_expr cont = (ovs_expr){ OVS_FUNCTION, .p=e->cont };

	compile_state* s;
	with_parameters(e->state, e->params);

	i->size = 3;
	i->values[0] = ovs_alias(cont);
	i->values[1] = statement_function(s, &statement_with_lambda_function);
	i->values[2] = statement_function(s, &statement_with_variable_function);

	return OVRU_SUCCESS;
}

int32_t compile_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	ovs_expr params = args[1];
	ovs_expr body = args[2];
	ovs_expr cont = args[3];

	compile_state* s = make_compile_state(NULL, f->context, cont.p);
	ovs_expr empty = ovs_root_symbol(OVS_DATA_NIL)->expr;

	i->size = 3;
	i->values[0] = ovs_alias(params);
	i->values[1] = parameters_function(s, empty, body.p, &parameters_with_function);
	i->values[2] = parameters_function(s, empty, body.p, &parameters_end_function);

	return OVRU_SUCCESS;
}

ovs_expr ovru_compiler(ovs_context* c) {
	return ovs_function(c, &compile_function, 0, NULL);
}

