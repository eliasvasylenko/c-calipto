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

/*
 *
 *
 *
 * TODO the story for variable bindings is broken!
 *
 * We have a stack of compile_states, starting with the root lambda we create a new one
 * to process each term in the body of the lambda which is also a lambda. The context
 * contains the parameters of the lambda, so we can search up through the stack to see
 * what's in the lexical scope.
 *
 * When we encounter a variable at the top of the stack which isn't in the top context, we
 * search up, then when we find it we add it as a capture in each descendent scope until
 * we make our way back to the top. This can mutate any number of contexts in the stack.
 *
 * Currently the bdtrie containing var bindings for a context is shared between all builder
 * functions which can add captures to that context.
 *
 * This is fine for the normal path where we use builder functions linearly... but what if
 * two builder functions are used in parallelto add different lambda terms to the same
 * context? (this is not thread safe either)
 *
 * TODO Option 1)
 * TODO rather than reach down into the stack of contexts and mutating them all to bubble the
 * capture back up... we only track captures at the TOP of the stack, then propagate them down
 * as we complete terms. We can still keep the results of our search at the top of the stack
 * (e.g. capture made from lambda at depth -3 parameter index 5) so that we don't have to
 * repeat the work of bdtrie lookup.
 *
 *
 * TODO Option 2)
 * TODO alternatively we can use an entirely different data structure. A persistent immutable
 * trie, with cheap copying and ref counted nodes.
 *
 *
 * TODO whichever option we choose:
 *
 * perhaps we can lazily copy the context / create a new context every time a capture is added 
 * (and thus the context mutated). we can then avoid copying when there is only 1 ref to the
 * context. (must test that actually works and there aren't spurious refs hanging around
 * preventing us from this. It's an important class of optimisation which can apply in other
 * places too.
 *
 *
 *
 *
 *
 *
 * Perhaps Option 1) is simpler for the moment. Though we probably need something like 2) at
 * some point anyway to support scoped records in the language.
 *
 *
 *
 */

compile_state* make_compile_state(compile_state* parent, ovs_context* oc, ovs_expr params) {
	compile_state* c = malloc(sizeof(compile_state));
	c->counter = ATOMIC_VAR_INIT(0);

	c->parent = parent;
	c->ovs_context = oc;

	c->variables = (bdtrie){
		NULL,
		get_variable_binding,
		update_variable_binding,
		free_variable_binding
	};
	c->capture_count = 0;
	c->param_count = 0;
	c->params = ovs_alias(params);
	while (!ovs_is_symbol(params)) {
		ovs_expr head = ovs_car(params);
		ovs_expr tail = ovs_cdr(params);
		ovs_dealias(params);
		params = tail;

		ovru_variable v = { OVRU_PARAMETER, c->bindings.param_count };
		bdtrie_insert(&c->bindings.variables, sizeof(ovs_expr_ref*), head.p, &v);
		ovs_dealias(head);

		c->bindings.param_count++;
	}
	ovs_dealias(params);

	c->body.term_count = 0;

	return c;
}

/*
 * API
 */

ovs_expr statement_function(statement_data* d, ovs_function_type* t) {
	statement_data** s;
	ovs_expr e = ovs_function(d->context->ovs_context, t, sizeof(statement_data*), (void**)&s);
	*s = d;
	return e;
}

ovs_expr parameters_function(parameters_data* d, ovs_context* c, ovs_function_type* t) {
	parameters_data** p;
	ovs_expr e = ovs_function(c, t, sizeof(parameters_data*), (void**)&p);
	*p = d;
	return e;
}

void statement_with(ovs_instruction* i, ovs_expr r, statement_data* e, ovs_expr cont, ovru_term t) {
	statement_data* d = malloc(sizeof(statement_data));
	d->context = e->context;
	d->term_origin = ovs_ref(r.p);
	d->term = t;
	d->cont = e->cont;

	i->size = 5;
	i->values[0] = ovs_alias(cont);
	i->values[1] = statement_function(d, &statement_with_lambda_function);
	i->values[2] = statement_function(d, &statement_with_variable_function);
	i->values[3] = statement_function(d, &statement_with_quote_function);
	i->values[4] = statement_function(d, &statement_end_function);
}

ovru_statement unroll_statement(statement_data* d, int32_t size) {
	if (d->term_origin == NULL) {
		return (ovru_statement){ 0, malloc(sizeof(ovru_term) * size) };
	}

	statement_data* p = ovs_function_extra_data(&d->term_origin->function);
	ovru_statement s = unroll_statement(p, size + 1);
	s.terms[s.term_count] = d->term;
	s.term_count++;
	return s;
}

int32_t statement_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* data = ovs_function_extra_data(f);

	ovru_statement s = unroll_statement(data, 0);

	ovru_lambda* l = malloc(sizeof(ovru_lambda));
	l->param_count = param_count;
	l->params = params;
	l->capture_count = lambda_context.bindings.capture_count;
	l->captures = lambda_context.bindings.captures;
	l->body = body;

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
	ovru_result r = find_variable(&t.variable, e->context, variable.p);

	if (r == OVRU_SUCCESS) {
		statement_with(i, receiver, e, cont, t);
	}
	return r;
}

int32_t statement_with_quote_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	statement_data* e = ovs_function_extra_data(f);

	ovs_expr receiver = args[0];
	ovs_expr data = args[1];
	ovs_expr cont = args[2];

	ovru_term t = { .quote=data };

	statement_with(i, receiver, e, cont, t);
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

	const ovs_expr_ref** param_list;
	int32_t param_count = ovs_delist_of(&f->context->root_tables[OVS_UNQUALIFIED], e->params, (void***)&param_list, get_ref);
	d->context = malloc(sizeof(compile_state));
	d->context->parent = e->context;
	d->context->ovs_context = f->context;
	d->context->bindings = make_variable_bindings(param_count, param_list);

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
	d->counter = ATOMIC_VAR_INIT(2);
	d->params = ovs_root_symbol(OVS_DATA_NIL)->expr;
	d->context = NULL;
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

