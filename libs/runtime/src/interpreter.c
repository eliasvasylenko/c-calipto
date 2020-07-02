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
#include "c-ohvu/runtime/interpreter.h"

typedef struct variable_bindings {
	uint32_t capture_count;
	uint32_t param_count;
	ovru_variable* captures;
	bdtrie variables;
} variable_bindings;

void* get_variable_binding(uint32_t key_size, void* key_data, void* value_data, bdtrie_node* owner) {
	ovru_variable* value = malloc(sizeof(ovru_variable*));
	*value = *((ovru_variable*)value_data);
	return value;
}

void update_variable_binding(void* value, bdtrie_node* owner) {}

void free_variable_binding(void* value) {
	free(value);
}

variable_bindings make_variable_bindings(uint64_t param_count, const ovs_expr_ref** params) {
	variable_bindings b = {
		0,
		param_count,
		NULL,
		{
			NULL,
			get_variable_binding,
			update_variable_binding,
			free_variable_binding
		}
	};
	for (int i = 0; i < param_count; i++) {
		ovru_variable v = { OVRU_PARAMETER, i };
		bdtrie_insert(&b.variables, sizeof(ovs_expr_ref*), params + i, &v);
	}
	return b;
}

typedef struct compile_context {
	struct compile_context* parent;
	variable_bindings bindings;
	ovs_expr data_quote;
	ovs_expr data_lambda;
} compile_context;

ovru_result find_variable(ovru_variable* result, compile_context* c, ovs_expr_ref* symbol) {
	ovru_variable v = { OVRU_CAPTURE, c->bindings.capture_count };
	ovru_variable w = *(ovru_variable*)bdtrie_find_or_insert(&c->bindings.variables, sizeof(ovs_expr_ref*), &symbol, &v).data;

	if (w.index == v.index && w.type == v.type) {
		if (c->parent == NULL) {
			return OVRU_VARIABLE_NOT_IN_SCOPE;
		}
		ovru_variable capture;
		ovru_result r = find_variable(&capture, c->parent, symbol);
		if (r != OVRU_SUCCESS) {
			return r;
		}

		c->bindings.capture_count++;
	
		ovru_variable* old_captures = c->bindings.captures;
		c->bindings.captures = malloc(sizeof(ovru_variable*) * c->bindings.capture_count);
		if (old_captures != NULL) {
			memcpy(c->bindings.captures, old_captures, sizeof(uint32_t*) * (c->bindings.capture_count - 1));
			free(old_captures);
		}

		c->bindings.captures[c->bindings.capture_count - 1] = capture;
	}

	*result = w;
	return OVRU_SUCCESS;
}

ovru_result compile_quote(ovru_term* result, int32_t part_count, ovs_expr* parts, compile_context* c) {
	if (part_count != 1) {
		return OVRU_INVALID_QUOTE_LENGTH;
	}

	*result = (ovru_term){ .quote=parts[0] };

	return OVRU_SUCCESS;
}

ovru_result compile_statement(ovru_statement* result, ovs_expr s, compile_context* c);

ovru_lambda* ref_lambda(ovru_lambda* l) {
	atomic_fetch_add(&l->ref_count, 1);
	return l;
}

void free_lambda(ovru_lambda* l) {
	if (atomic_fetch_add(&l->ref_count, -1) > 1) {
		return;
	}
	if (l->param_count > 0) {
		for (int i = 0; i < l->param_count; i++) {
			ovs_free(OVS_SYMBOL, l->params[i]);
		}
		free(l->params);
	}
	if (l->capture_count > 0) {
		free(l->captures);
	}
	ovru_free(l->body);
	free(l);
}

void* get_ref(ovs_expr e) {
	return ovs_ref(e.p);
}

ovru_result compile_lambda(ovru_term* result, int32_t part_count, ovs_expr* parts, compile_context* c) {
	if (part_count != 2) {
		return OVRU_INVALID_LAMBDA_LENGTH;
	}

	ovs_expr params_decl = parts[0];
	ovs_expr body_decl = parts[1];

	ovs_expr_ref** params;
	int32_t param_count = ovs_delist_of(params_decl, (void***)&params, get_ref);
	if (param_count < 0) {
		return OVRU_INVALID_PARAMETER_TERMINATOR;
	}

	compile_context lambda_context = {
		c,
		make_variable_bindings(param_count, (const ovs_expr_ref**) params),
		c->data_quote,
		c->data_lambda
	};

	ovru_statement body;
	ovru_result success = compile_statement(&body, body_decl, &lambda_context);

	if (success == OVRU_SUCCESS) {
		ovru_lambda* l = malloc(sizeof(ovru_lambda));
		l->ref_count = ATOMIC_VAR_INIT(1);
		l->param_count = param_count;
		l->params = params;
		l->capture_count = lambda_context.bindings.capture_count;
		l->captures = lambda_context.bindings.captures;
		l->body = body;
		*result = (ovru_term){ .type=OVRU_LAMBDA, .lambda=l };
	} else {
		for (int i = 0; i < param_count; i++) {
			ovs_free(OVS_SYMBOL, params[i]);
		}
		if (param_count > 0) {
			free(params);
		}
		if (lambda_context.bindings.capture_count > 0) {
			free(lambda_context.bindings.captures);
		}
	}

	bdtrie_clear(&lambda_context.bindings.variables);

	return success;
}

ovru_result compile_expression(ovru_term* result, ovs_expr e, compile_context* c) {
	if (ovs_atom(e)) {
		ovru_variable v;
		ovru_result r = find_variable(&v, c, e.p);
		if (r == OVRU_SUCCESS) {
			*result = (ovru_term){ .type=OVRU_VARIABLE, .variable=v };
		}
		return r;
	}

	ovs_expr* parts;
	uint32_t count = ovs_delist(e, &parts);
	if (count <= 0) {
		return count == 0 ? OVRU_EMPTY_EXPRESSION : OVRU_INVALID_EXPRESSION_TERMINATOR;
	}

	ovs_expr kind = parts[0];
	int32_t term_count = count - 1;
	ovs_expr* terms = parts + 1;

	ovru_result success;
	if (ovs_eq(kind, c->data_quote)) {
		success = compile_quote(result, term_count, terms, c);

	} else if (ovs_eq(kind, c->data_lambda)) {
		success = compile_lambda(result, term_count, terms, c);

	} else {
		success = OVRU_INVALID_EXPRESSION_TYPE;
	}

	for (int i = 0; i < count; i++) {
		ovs_dealias(parts[i]);
	}
	free(parts);
	
	return success;
}

ovru_result compile_statement(ovru_statement* result, ovs_expr s, compile_context* c) {
	ovs_expr* expressions;
	int32_t count = ovs_delist(s, &expressions);
	if (count <= 0) {
		return count == 0 ? OVRU_EMPTY_STATEMENT : OVRU_INVALID_STATEMENT_TERMINATOR;
	}

	ovru_term* terms = malloc(sizeof(ovru_term) * count);
	ovru_result success;
	for (int i = 0; i < count; i++) {
		success = compile_expression(&terms[i], expressions[i], c);
		ovs_dealias(expressions[i]);

		if (success != OVRU_SUCCESS) {
			for (i++; i < count; i++) {
				ovs_dealias(expressions[i]);
			}
			break;
		}
	}
	free(expressions);

	if (success != OVRU_SUCCESS) {
		free(terms);
	}

	*result = (ovru_statement){ count, terms };

	return success;
}

ovru_result ovru_compile(ovru_statement* result, const ovs_expr e, const uint32_t param_count, const ovs_expr_ref** params) {
	ovs_expr data = ovs_symbol(NULL, ovio_u_strref(u"data"));
	compile_context c = {
		NULL,
		make_variable_bindings(param_count, params),
		ovs_symbol(data.p, ovio_u_strref(u"quote")),
		ovs_symbol(data.p, ovio_u_strref(u"lambda"))
	};
	ovs_dealias(data);

	ovru_result success = compile_statement(result, e, &c);

	if (c.bindings.capture_count > 0) {
		free(c.bindings.captures);
	}
	bdtrie_clear(&c.bindings.variables);

	ovs_dealias(c.data_quote);
	ovs_dealias(c.data_lambda);

	return success;
}

#define ARGS_ON_STACK 16
#define ARGS_ON_HEAP 32

typedef struct instruction_slot {
	ovs_expr stack_values[ARGS_ON_STACK];
	uint32_t heap_values_size;
	ovs_expr* heap_values;
	ovs_instruction instruction;
	struct instruction_slot* next;
} instruction_slot;

void prepare_instruction_slot(instruction_slot* i, uint32_t size) {
	i->instruction.size = size;
	if (size <= ARGS_ON_STACK) {
		i->instruction.values = i->stack_values;
	} else {
		if (size > i->heap_values_size) {
			if (i->heap_values_size > 0) {
				free(i->heap_values);
			}
			i->heap_values_size = size;
			i->heap_values = malloc(sizeof(ovs_expr) * size);
		}
		i->instruction.values = i->heap_values;
	}
}

void clear_instruction_slot(instruction_slot* s) {
	for (int i = 0; i < s->instruction.size; i++) {
		ovs_dealias(s->instruction.values[i]);
	}
}

ovs_expr represent_param(void* p) {
	return (ovs_expr){ OVS_SYMBOL, .p=*(ovs_expr_ref**)p };
}

ovs_expr represent_bound_lambda(void* d) {
	ovru_bound_lambda* l = d;

	ovs_expr form[] = {
		ovs_list_of(l->lambda->param_count, (void**)l->lambda->params, represent_param),
		ovs_list(0, NULL) // TODO
	};
	uint32_t size = sizeof(form) / sizeof(ovs_expr);

	ovs_expr r = ovs_list(2, form);

	for (int i = 0; i < size; i++) {
		ovs_dealias(form[i]);
	}

	return r;
}

ovs_function_info inspect_bound_lambda(void* d) {
	ovru_bound_lambda* l = d;

	return (ovs_function_info){ l->lambda->param_count, l->lambda->body.term_count };
}

void eval_statement(ovs_instruction* result, ovru_statement s, const ovs_expr* args, const ovs_expr* closure);

int32_t apply_bound_lambda(ovs_instruction* result, ovs_expr* args, void* d) {
	ovru_bound_lambda* l = d;

	eval_statement(result, l->lambda->body, args, l->closure);

	return OVRU_SUCCESS;
}

void free_bound_lambda(void* d) {
	ovru_bound_lambda* l = d;
	for (int i = 0; i < l->lambda->capture_count; i++) {
		ovs_dealias(l->closure[i]);
	}
	free(l->closure);
	free_lambda(l->lambda);
}

static ovs_function_type lambda_function = {
	u"lambda",
	represent_bound_lambda,
	inspect_bound_lambda,
	apply_bound_lambda,
	free_bound_lambda
};

void eval_expression(ovs_expr* result, ovru_term e, const ovs_expr* args, const ovs_expr* closure) {
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
			ovru_bound_lambda l = { ref_lambda(e.lambda), c };

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
			*result = ovs_function(&lambda_function, sizeof(ovru_bound_lambda), &l);
			break;

		default:
			*result = ovs_alias(e.quote);
			break;
	}
}

void eval_statement(ovs_instruction* result, ovru_statement s, const ovs_expr* args, const ovs_expr* closure) {
	for (int i = 0; i < s.term_count; i++) {
		eval_expression(result->values + i, s.terms[i], args, closure);
	}
}

ovru_result execute_instruction(instruction_slot* next, instruction_slot* current) {
	if (current->instruction.values[0].type != OVS_FUNCTION) {
		return OVRU_ATTEMPT_TO_CALL_NON_FUNCTION;
	}
	
	ovs_function_data* f = &current->instruction.values[0].p->function;

	ovs_function_info i = f->type->inspect(f + 1);

	prepare_instruction_slot(next, i.max_result_size);

	if (i.arg_count != current->instruction.size - 1) {
		return OVRU_ARGUMENT_COUNT_MISMATCH;
	}

	ovru_result r = f->type->apply(&next->instruction, current->instruction.values + 1, f + 1);

	clear_instruction_slot(current);

	return r;
}

ovru_result ovru_eval(const ovru_statement s, const ovs_expr* args) {
	/*
	 * We have two instruction slots, the current instruction which is
	 * executing, and the next instruction which is being written. We
	 * flip-flop between them, like double buffering.
	 */

	instruction_slot a;
	instruction_slot b;
	a.heap_values_size = 0;
	b.heap_values_size = 0;
	a.next = &b;
	b.next = &a;

	instruction_slot* current = &a;
	prepare_instruction_slot(current, s.term_count);
	eval_statement(&current->instruction, s, args, NULL);

	ovru_result r = OVRU_SUCCESS;
	while (r == OVRU_SUCCESS && current->instruction.size > 0) {
		r = execute_instruction(current->next, current);
		current = current->next;
	}

	clear_instruction_slot(current);

	if (a.heap_values_size > 0) {
		free(a.heap_values);
	}
	if (b.heap_values_size > 0) {
		free(b.heap_values);
	}

	return r;
}

void ovru_free(ovru_statement s) {
	if (s.terms != NULL) {
		for (int i = 0; i < s.term_count; i++) {
			if (s.terms[i].type == OVRU_LAMBDA) {
				free_lambda(s.terms[i].lambda);
			}
		}
		free(s.terms);
	}
}

