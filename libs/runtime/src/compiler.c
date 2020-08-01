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

compile_state* make_compile_state(compile_state* parent, ovs_context* oc, const ovs_expr_ref* r) {
	compile_state* s = malloc(sizeof(compile_state));
	s->counter = ATOMIC_VAR_INIT(0);

	s->parent = parent;
	s->context = oc;

	s->variables = (bdtrie){
		NULL,
		get_variable_binding,
		update_variable_binding,
		free_variable_binding
	};
	s->capture_count = 0;
	s->param_count = -1;

	s->body.term_count = 0;

	s->cont = ovs_ref(r);

	return s;
}

void add_parameters(compile_state* s, ovs_expr params) {
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

/*
 * API
 */

ovs_expr statement_function(compile_state* s, ovs_function_type* t) {
	statement_data* d;
	ovs_expr e = ovs_function(s->context, t, sizeof(statement_data*), (void**)&d);
	d->state = s;
	return e;
}

ovs_expr parameters_function(parameters_data* d, ovs_context* c, ovs_function_type* t) {
	parameters_data** p;
	ovs_expr e = ovs_function(c, t, sizeof(parameters_data*), (void**)&p);
	*p = d;
	return e;
}

void statement_with(
		ovs_instruction* i,
		statement_data* e,
		ovs_expr cont,
		ovru_term t,
		int32_t capture_count, variable_capture* captures) {
	compile_state* s = e->state;
	if (atomic_load(&s->counter) > 1) {
		s = make_compile_state(e->state, e->state->context, e->state->cont);
	}
	ovru_statement b = s->body;
	s->body.term_count++;
	s->body.terms = malloc(sizeof(ovru_term) * s->body.term_count);
	if (b.term_count > 0) {
		for (int i = 0; i < b.term_count; i++) {
			s->body.terms[i] = b.terms[i];
		}
		free(b.terms);
	}
	s->body.terms[b.term_count] = t;

	/*
	 * TODO add captures from new term
	 */

	i->size = 5;
	i->values[0] = ovs_alias(cont);
	i->values[1] = statement_function(s, &statement_with_lambda_function);
	i->values[2] = statement_function(s, &statement_with_variable_function);
	i->values[3] = statement_function(s, &statement_with_quote_function);
	i->values[4] = statement_function(s, &statement_end_function);
}

ovru_lambda* flatten_compiler_state(compile_state* s, int32_t term_count, int32_t capture_count) {
	term_count += s->body.term_count;
	capture_count += s->capture_count;

	ovru_lambda* l;

	if (s->param_count >= 0) {
		l = malloc(sizeof(ovru_lambda));

		l->param_count = s->param_count;
		l->params = malloc(sizeof(ovs_expr) * l->param_count);

		l->capture_count = capture_count;
		l->captures = malloc(sizeof(ovru_variable) * l->capture_count);

		l->body.term_count = term_count;
		l->body.terms = malloc(sizeof(ovru_term) * l->body.term_count);

	} else {
		l = flatten_compiler_state(s->parent, term_count, capture_count);
	}

	ovru_variable* captures = l->captures + l->capture_count - capture_count;
	for (int i = 0; i < s->capture_count; i++) {
		/*
		 * TODO only put captures with depth 0 here?
		 *
		 * TODO propagate the rest down?
		 */
		captures[i] = s->captures[i];
	}

	ovru_term* terms = l->terms + l->term_count - term_count;
	for (int i = 0; i < s->term_count; i++) {
		term[i] = s->term[i];
	}

	return l;
}

int32_t statement_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* data = ovs_function_extra_data(f);

	ovru_lambda* l = flatten_compiler_state(data->state, 0, 0);

	ovru_term t = (ovru_term){ .type=OVRU_LAMBDA, .lambda=l };

	if (data->context->parent == NULL) {
		// TODO make function from lambda
		ovs_function f;

		ovru_bound_lambda* b;

		i->size = 2;
		i->values[0] = ovs_alias(data->cont);
		i->values[1] = ovs_function(
				data->context->ovs_context,
				&lambda_function,
				sizeof(ovru_bound_lambda),
				(void**)&b);

		b->lambda = l;
		b->closure = NULL;

	} else {
		// TODO statament_with on parent continue
		ovru_term t; // = lambda

		statement_with(i, data, data_cont, t, propagated_capture_count, propagated_captures);
	}

	return 91546;
}

int32_t statement_with_lambda_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* data = ovs_function_extra_data(f);

	ovs_expr params = args[1];
	ovs_expr body = args[2];
	ovs_expr cont = args[3];

	parameters_data* d = malloc(sizeof(parameters_data));
	d->counter = ATOMIC_VAR_INIT(2);
	d->params = ovs_root_symbol(OVS_DATA_NIL)->expr;
	d->context = data->context;
	d->statement_cont = body.p;
	d->cont = cont.p;

	i->size = 3;
	i->values[0] = ovs_alias(params);
	i->values[1] = parameters_function(d, f->context, &parameters_with_function);
	i->values[2] = parameters_function(d, f->context, &parameters_end_function);
}

int32_t statement_with_variable_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* e = ovs_function_extra_data(f);

	ovs_expr receiver = args[0];
	ovs_expr variable = args[1];
	ovs_expr cont = args[2];

	ovru_term t;
	t.type = OVRU_VARIABLE;
	uint32_t depth = variable.type = SYMBOL
		? find_variable(&t.variable, e->context, variable.p)
		: -1;

	if (depth >= 0) {
		variable_capture captures[] = { { t.variable, depth } };
		statement_with(i, e, cont, t, 1, captures);

	} else {
		// TODO how to deal with errors???
	}

	return r;
}

int32_t statement_with_quote_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* e = ovs_function_extra_data(f);

	ovs_expr receiver = args[0];
	ovs_expr data = args[1];
	ovs_expr cont = args[2];

	ovru_term t = { .quote=data };

	statement_with(i, e, cont, t, 0, NULL);
	return OVRU_SUCCESS;
}

int32_t parameters_with_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	parameters_data* e = ovs_function_extra_data(f);

	ovs_expr param = args[1];
	ovs_expr cont = args[2];

	parameters_data* d = malloc(sizeof(parameters_data));
	d->counter = ATOMIC_VAR_INIT(2);
	d->params = ovs_cons(&f->context->root_tables[OVS_UNQUALIFIED], param, e->params);
	d->context = e->context;
	d->statement_cont = e->statement_cont;
	d->cont = e->cont;

	i->size = 3;
	i->values[0] = ovs_alias(cont);
	i->values[1] = parameters_function(d, f->context, &parameters_with_function);
	i->values[2] = parameters_function(d, f->context, &parameters_end_function);

	return OVRU_SUCCESS;
}

int32_t parameters_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	parameters_data* e = ovs_function_extra_data(f);

	ovs_expr cont = (ovs_expr){ OVS_FUNCTION, .p=e->statement_cont };

	statement_data* d = malloc(sizeof(statement_data));
	d->counter = ATOMIC_VAR_INIT(2);
	d->context = malloc(sizeof(compile_state));
	d->term_origin = NULL;
	d->cont = e->cont;

	d->state = make_compile_state(NULL, f->context);
	add_parameters(d->state, e->params);

	i->size = 3;
	i->values[0] = ovs_alias(cont);
	i->values[1] = statement_function(d, &statement_with_lambda_function);
	i->values[2] = statement_function(d, &statement_with_variable_function);

	return OVRU_SUCCESS;
}

int32_t compile_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	ovs_expr params = args[1];
	ovs_expr body = args[2];
	ovs_expr cont = args[3];

	parameters_data* d = malloc(sizeof(parameters_data));
	d->params = ovs_root_symbol(OVS_DATA_NIL)->expr;
	d->state = NULL;
	d->statement_cont = body.p;
	d->cont = cont.p;

	i->size = 3;
	i->values[0] = ovs_alias(params);
	i->values[1] = parameters_function(d, f->context, &parameters_with_function);
	i->values[2] = parameters_function(d, f->context, &parameters_end_function);

	return OVRU_SUCCESS;
}

ovs_expr ovru_compiler(ovs_context* c) {
	return ovs_function(c, &compile_function, 0, NULL);
}

