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

void compile_state_without_parameters(compile_state* s) {
	s->total_capture_count = s->parent->total_capture_count;
	s->propagated_capture_count = s->parent->propagated_capture_count;

	s->param_count = -1;
}

void compile_state_with_parameters(compile_state* s, ovs_expr params) {
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

void compile_state_with_captures(compile_state* s, uint32_t capture_count, variable_capture* captures) {
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

void compile_state_with_term(compile_state* s, ovru_term t) {
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

