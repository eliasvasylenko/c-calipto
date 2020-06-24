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

typedef struct variable_binding {
	const ovs_expr_ref* symbol;
	ovru_variable variable;
} variable_binding;

void* get_variable_binding(uint32_t key_size, void* key_data, bdtrie_node* owner) {
	ovru_variable* value = malloc(sizeof(ovru_variable*));
	*value = ((variable_binding*)key_data)->variable;
	return value;
}

void update_variable_binding(void* value, bdtrie_node* owner) {}

void free_variable_binding(void* value) {
	free(value);
}

typedef struct compile_context {
	variable_bindings variables;
	ovs_expr data_quote;
	ovs_expr data_lambda;
} compile_context;

ovru_result compile_quote(ovru_term* result, int32_t part_count, ovs_expr* parts, compile_context c) {
	if (part_count != 1) {
		return OVRU_INVALID_QUOTE_LENGTH;
	}

	*result = (ovru_term){ .quote=parts[0] };

	return OVRU_SUCCESS;
}

ovru_result compile_statement(ovru_statement* result, ovs_expr s, compile_context c);

ovru_term ovs_alias_term(ovru_term t) {
	switch (t.type) {
		case OVRU_LAMBDA:
			ovru_ref_lambda(t.lambda);
			break;
		case OVRU_VARIABLE:
			break;
		default:
			ovs_alias(t.quote);
			break;
	}
	return t;
}

void ovs_dealias_term(ovru_term t) {
	switch (t.type) {
		case OVRU_LAMBDA:
			ovru_free_lambda(t.lambda);
			break;
		case OVRU_VARIABLE:
			break;
		default:
			ovs_dealias(t.quote);
			break;
	}
}

void* get_ref(ovs_expr e) {
	return e.p;
}

ovru_result compile_lambda(ovru_term* result, int32_t part_count, ovs_expr* parts, compile_context c) {
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

	ovru_statement body;
	ovru_result success = compile_statement(&body, body_decl, c);

	uint32_t var_count = 0;
	uint32_t* vars = NULL;

	if (success == OVRU_SUCCESS) {
		ovru_lambda* l = malloc(sizeof(ovru_lambda));
		l->ref_count = ATOMIC_VAR_INIT(1);
		l->param_count = param_count;
		if (param_count > 0) {
			l->params = malloc(sizeof(ovs_expr_ref*) * param_count);
			for (int i = 0; i < param_count; i++) {
				ovs_ref(params[i]);
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
		l->body = body;
		*result = (ovru_term){ .type=OVRU_LAMBDA, .lambda=l };
	}

	return success;
}

ovru_lambda* ovru_ref_lambda(ovru_lambda* l) {
	atomic_fetch_add(&l->ref_count, 1);
}

void ovru_free_lambda(ovru_lambda* l) {
	if (atomic_fetch_add(&l->ref_count, -1) > 1) {
		if (l->param_count > 0) {
			for (int i = 0; i < l->param_count; i++) {
				ovs_free(OVS_SYMBOL, l->params[i]);
			}
			free(l->params);
		}
		if (l->var_count > 0) {
			free(l->vars);
		}
		if (l->body.term_count > 0) {
			for (int i = 0; i < l->body.term_count; i++) {
				ovs_dealias_term(l->body.terms[i]);
			}
			free(l->body.terms);
		}
	}
}

ovru_result compile_expression(ovru_term* result, ovs_expr e, compile_context c) {
	if (ovs_atom(e)) {
		uint32_t* index_into_parent = bdtrie_find(
				&c.variables.variables,
				sizeof(ovs_expr_ref*),
				e.p).data;
		*result = (ovru_term){ .type=OVRU_VARIABLE, .variable=*index_into_parent };
		return OVRU_SUCCESS;
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
	if (ovs_eq(kind, c.data_quote)) {
		success = compile_quote(result, term_count, terms, c);

	} else if (ovs_eq(kind, c.data_lambda)) {
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

ovru_result compile_statement(ovru_statement* result, ovs_expr s, compile_context c) {
	ovs_expr* expressions;
	int32_t count = ovs_delist(s, &expressions);
	if (count <= 0) {
		return count == 0 ? OVRU_EMPTY_STATEMENT : OVRU_INVALID_STATEMENT_TERMINATOR;
	}

	printf("count %i\n", count);

	ovru_term* terms = malloc(sizeof(ovru_term) * count);
	ovru_result success;
	for (int i = 0; i < count; i++) {
		printf("%p\n", &terms[i]);
		printf("%i\n", expressions[i].type);
		success = compile_expression(&terms[i], expressions[i], c);
		ovs_dealias(expressions[i]);

		if (success != OVRU_SUCCESS) {
			for (; i < count; i++) {
				ovs_dealias(expressions[i]);
			}
			break;
		}
	}
	free(expressions);

	if (success == OVRU_SUCCESS) {
		*result = (ovru_statement){ count, terms };
	}

	return success;
}

void capture_variable(bdtrie p, variable_bindings* b, ovs_expr_ref* symbol) {
	if (bdtrie_find(&b->variables, sizeof(ovs_expr_ref*), &symbol).data == NULL) {
		variable_binding v = { symbol, { true, b->capture_count++ } };
		bdtrie_insert(&b->variables, sizeof(ovs_expr_ref*), &v);

		ovru_variable* old_captures = b->captures;
		b->captures = malloc(sizeof(ovru_variable*) * b->capture_count);
		if (old_captures != NULL) {
			memcpy(b->captures, old_captures, sizeof(uint32_t*) * (b->capture_count - 1));
			free(old_captures);
		}

		ovru_variable* capture = bdtrie_find(&p, sizeof(ovs_expr_ref*), &symbol).data;
		if (capture == NULL) {
			// TODO ERROR
		}
		b->captures[b->capture_count - 1] = *capture;
	}
}

ovru_result ovru_compile(ovru_statement* result, const ovs_expr e, const uint32_t param_count, const ovs_expr_ref** params) {
	bdtrie variables = (bdtrie){
		NULL,
		get_variable_binding,
		update_variable_binding,
		free_variable_binding
	};

	variable_bindings b = {
		0,
		param_count,
		NULL,
		variables
	};
	for (int i = 0; i < param_count; i++) {
		variable_binding v = { params[i], { OVRU_PARAMETER, i } };
		bdtrie_insert(&variables, sizeof(ovs_expr_ref*), &v);
	}

	ovs_expr data = ovs_symbol(NULL, ovio_u_strref(u"data"));
	compile_context c = {
		b,
		ovs_symbol(data.p, ovio_u_strref(u"quote")),
		ovs_symbol(data.p, ovio_u_strref(u"lambda"))
	};
	ovs_dealias(data);

	ovru_result success = compile_statement(result, e, c);

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

ovs_expr represent_param(void* p) {
	return (ovs_expr){ OVS_SYMBOL, .p=*(ovs_expr_ref**)p };
}

ovs_expr represent_lambda(void* d) {
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

ovs_function_info inspect_lambda(void* d) {
	ovru_bound_lambda* l = d;

	return (ovs_function_info){ l->lambda->param_count, l->lambda->body.term_count };
}

int32_t apply_lambda(ovs_instruction* result, ovs_expr* args, void* d) {
	ovru_bound_lambda* l = d;
	;
}

void free_lambda(void* d) {
	ovru_bound_lambda* l = d;
	for (int i = 0; i < l->lambda->var_count; i++) {
		ovs_dealias(l->capture[i]);
	}
	free(l->capture);
	ovru_free_lambda(l->lambda);
}

static ovs_function_type lambda_function = {
	u"lambda",
	represent_lambda,
	inspect_lambda,
	apply_lambda,
	free_lambda
};

void eval_expression(ovs_expr* result, ovru_term e, const ovs_expr* args, const ovs_expr* closure) {
	switch (e.type) {
		case OVRU_VARIABLE:
			switch (e.variable.type) {
				case OVRU_PARAMETER:
					*result = args[e.variable.index];

				case OVRU_CAPTURE:
					*result = closure[e.variable.index];

				default:
					assert(false);
			}

		case OVRU_LAMBDA:
			;
			ovru_lambda l = {
			};
			*result = ovs_function(&lambda_function, sizeof(ovru_lambda), &l);

		default:
			*result = ovs_alias(e.quote);
	}
}

void eval_statement(instruction_slot* result, ovru_statement s, const ovs_expr* args, const ovs_expr* closure) {
	prepare_instruction_slot(result, s.term_count);

	for (int i = 0; i < s.term_count; i++) {
		eval_expression(&result->instruction.values[i], s.terms[i], args, closure);
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

	return f->type->apply(&next->instruction, current->instruction.values, f + 1);
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
	eval_statement(current, s, args, NULL);

	ovru_result r = OVRU_SUCCESS;
	while (r == OVRU_SUCCESS && current->instruction.size > 0) {
		r = execute_instruction(current->next, current);
		current = current->next;
	}

	if (a.heap_values_size > 0) {
		free(a.heap_values);
	}
	if (b.heap_values_size > 0) {
		free(b.heap_values);
	}

	return r;
}

