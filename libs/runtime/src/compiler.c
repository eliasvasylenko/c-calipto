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
#include "c-ohvu/runtime/evaluator.h"
#include "c-ohvu/runtime/compiler.h"
#include "compilerapi.h"

/*
 * Eval Lambda
 */

void eval_expression(ovs_context* context, ovs_expr* result, ovru_term e, const ovs_expr* args, const ovs_expr* closure) {
	switch (e.type) {
		case OVRU_VARIABLE:
			switch (e.variable.type) {
				case OVRU_PARAMETER:
					*result = ovs_alias(args[e.variable.index]);
					break;

				case OVRU_CAPTURE:
					*result = ovs_alias(closure[e.variable.index]);
					break;

				default:
					assert(false);
			}
			break;

		case OVRU_LAMBDA:
			;
			ovs_expr* c = malloc(sizeof(ovs_expr) * e.lambda->capture_count);

			for (int i = 0; i < e.lambda->capture_count; i++) {
				ovru_variable capture = e.lambda->captures[i];

				switch (capture.type) {
					case OVRU_PARAMETER:
						c[i] = ovs_alias(args[capture.index]);
						break;

					case OVRU_CAPTURE:
						c[i] = ovs_alias(closure[capture.index]);
						break;

					default:
						assert(false);
				}
			}
			bound_lambda_data* l;
			*result = ovs_function(context, &bound_lambda_function, sizeof(bound_lambda_data), (void**)&l);
			*l = (bound_lambda_data){ ref_lambda(e.lambda), c };
			break;

		default:
			*result = ovs_alias(e.quote);
			break;
	}
}

void eval_statement(ovs_context* c, ovs_instruction* result, ovru_statement s, const ovs_expr* args, const ovs_expr* closure) {
	for (int i = 0; i < s.term_count; i++) {
		eval_expression(c, result->values + i, s.terms[i], args, closure);
	}
}

int32_t bound_lambda_apply(ovs_instruction* result, ovs_expr* args, const ovs_function_data* d) {
	const bound_lambda_data* l = ovs_function_extra_data(d);

	eval_statement(d->context, result, l->lambda->body, args + 1, l->closure);

	return OVRU_SUCCESS;
}

/*
 * Compile State
 */

compile_state* ref_compile_state(compile_state* s) {
	atomic_fetch_add(&s->counter, 1);
	return s;
}

void free_compile_state(compile_state* c) {
	if (atomic_fetch_add(&c->counter, -1) == 1) {
		free_compile_state(c->parent);

		if (c->capture_count > 0)
			free(c->captures);
		ovs_dealias(c->params);
		bdtrie_clear(&c->variables);

		if (c->body.term_count > 0) {
			for (int i = 0; i < c->body.term_count; i++) {
				ovru_dealias_term(c->body.terms[i]);
			}
			free(c->body.terms);
		}
		
		ovs_free(OVS_FUNCTION, c->cont);
	}
}

bool is_unique_compile_state(compile_state* s) {
	return atomic_load(&s->counter) == 1;
}

/*
 * Find the variable in the enclosing lexical scope of the current expression by traversing
 * down the stack of enclosing lambdas.
 *
 * If the variable is found, return its depth and set `result` to the variable's location
 * at that depth.
 *
 * If the variable is not found, return -1;
 */
uint32_t find_variable(ovru_variable* result, compile_state* c, const ovs_expr_ref* symbol) {
	bdtrie_value v = bdtrie_find(&c->variables, sizeof(ovs_expr_ref*), &symbol);

	if (bdtrie_is_present(v)) {
		*result = *(ovru_variable*)v.data;
		return 0;

	} else if (c->parent == NULL) {
		return -1;

	} else {
		int32_t r = find_variable(result, c->parent, symbol);
		return (c->param_count < 0 || r < 0) ? r : r + 1;
	}
}

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

