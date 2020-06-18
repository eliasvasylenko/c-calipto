#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-ohvu/stringref.h"
#include "c-ohvu/idtrie.h"
#include "c-ohvu/sexpr.h"
#include "c-ohvu/interpreter.h"

typedef struct variable_bindings {
	uint32_t capture_count;
	uint32_t param_count;
	s_variable* captures;
	idtrie variables;
} variable_bindings;

typedef struct variable_binding {
	const s_expr_ref* symbol;
	s_variable variable;
} variable_binding;

void* get_variable_binding(uint32_t key_size, void* key_data, idtrie_node* owner) {
	s_variable* value = malloc(sizeof(s_variable*));
	*value = ((variable_binding*)key_data)->variable;
	return value;
}

void update_variable_binding(void* value, idtrie_node* owner) {}

void free_variable_binding(void* value) {
	free(value);
}

typedef struct compile_context {
	variable_bindings variables;
	s_expr data_quote;
	s_expr data_lambda;
} compile_context;

bool compile_quote(s_term* result, int32_t part_count, s_expr* parts, compile_context c) {
	if (part_count != 1) {
		return false;
	}

	*result = (s_term){ .quote=parts[0] };

	return true;
}

bool compile_statement(s_statement* result, s_expr s, compile_context c);

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

void* get_ref(s_expr e) {
	return e.p;
}

bool compile_lambda(s_term* result, int32_t part_count, s_expr* parts, compile_context c) {
	if (part_count != 2) {
		return false;
	}

	s_expr params_decl = parts[0];
	s_expr body_decl = parts[1];

	s_expr_ref** params;
	int32_t param_count = s_delist_of(params_decl, (void***)&params, get_ref);
	if (param_count < 0) {
		return false;
	}

	s_statement body;
	bool success = compile_statement(&body, body_decl, c);

	uint32_t var_count = 0;
	uint32_t* vars = NULL;

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
		l->body = body;
		*result = (s_term){ .type=LAMBDA, .lambda=l };
	}

	return success;
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
		}
		if (l->body.term_count > 0) {
			for (int i = 0; i < l->body.term_count; i++) {
				s_dealias_term(l->body.terms[i]);
			}
			free(l->body.terms);
		}
	}
}

bool compile_expression(s_term* result, s_expr e, compile_context c) {
	if (s_atom(e)) {
		uint32_t* index_into_parent = idtrie_find(
				&c.variables.variables,
				sizeof(s_expr_ref*),
				e.p).data;
		*result = (s_term){ .type=VARIABLE, .variable=*index_into_parent };
		return true;
	}

	s_expr* parts;
	uint32_t count = s_delist(e, &parts);
	if (count <= 0) {
		printf("Syntax error in expression: ");
		s_dump(e);
		return false;
	}

	s_expr kind = parts[0];
	int32_t term_count = count - 1;
	s_expr* terms = parts + 1;

	bool success;
	if (s_eq(kind, c.data_quote)) {
		success = compile_quote(result, term_count, terms, c);

	} else if (s_eq(kind, c.data_lambda)) {
		success = compile_lambda(result, term_count, terms, c);

	} else {
		success = false;
	}

	for (int i = 0; i < count; i++) {
		s_dealias(parts[i]);
	}
	free(parts);
	
	return success;
}

bool compile_statement(s_statement* result, s_expr s, compile_context c) {
	s_expr* expressions;
	int32_t count = s_delist(s, &expressions);
	if (count <= 0) {
		printf("Syntax error in statement: ");
		s_dump(s);
		return false;
	}

	printf("count %i\n", count);

	s_term* terms = malloc(sizeof(s_term) * count);
	bool success = true;
	for (int i = 0; i < count; i++) {
		printf("%p\n", &terms[i]);
		printf("%i\n", expressions[i].type);
		if (compile_expression(&terms[i], expressions[i], c)) {
			s_dealias(expressions[i]);
		} else {
			success = false;
			break;
		}
	}
	free(expressions);

	if (success) {
		*result = (s_statement){ count, terms };
	}

	return success;
}

void capture_variable(idtrie p, variable_bindings* b, s_expr_ref* symbol) {
	if (idtrie_find(&b->variables, sizeof(s_expr_ref*), &symbol).data == NULL) {
		variable_binding v = { symbol, { true, b->capture_count++ } };
		idtrie_insert(&b->variables, sizeof(s_expr_ref*), &v);

		s_variable* old_captures = b->captures;
		b->captures = malloc(sizeof(s_variable*) * b->capture_count);
		if (old_captures != NULL) {
			memcpy(b->captures, old_captures, sizeof(uint32_t*) * (b->capture_count - 1));
			free(old_captures);
		}

		s_variable* capture = idtrie_find(&p, sizeof(s_expr_ref*), &symbol).data;
		if (capture == NULL) {
			// TODO ERROR
		}
		b->captures[b->capture_count - 1] = *capture;
	}
}

s_result s_compile(s_statement* result, const s_expr e, const uint32_t param_count, const s_expr_ref** params) {
	idtrie variables = (idtrie){
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
		variable_binding v = { params[i], { PARAMETER, i } };
		idtrie_insert(&variables, sizeof(s_expr_ref*), &v);
	}

	s_expr data = s_symbol(NULL, u_strref(u"data"));
	compile_context c = {
		b,
		s_symbol(data.p, u_strref(u"quote")),
		s_symbol(data.p, u_strref(u"lambda"))
	};
	s_dealias(data);

	compile_statement(result, e, c);

	s_dealias(c.data_quote);
	s_dealias(c.data_lambda);

	return S_SUCCESS;
}

#define ARGS_ON_STACK 16
#define ARGS_ON_HEAP 32

typedef struct instruction_slot {
	s_expr stack_values[ARGS_ON_STACK];
	uint32_t heap_values_size;
	s_expr* heap_values;
	s_instruction instruction;
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
			i->heap_values = malloc(sizeof(s_expr) * size);
		}
		i->instruction.values = i->heap_values;
	}
}

s_expr represent_param(void* p) {
	return (s_expr){ SYMBOL, .p=*(s_expr_ref**)p };
}

s_expr represent_lambda(void* d) {
	s_bound_lambda* l = d;

	s_expr form[] = {
		s_list_of(l->lambda->param_count, (void**)l->lambda->params, represent_param),
		s_list(0, NULL) // TODO
	};
	uint32_t size = sizeof(form) / sizeof(s_expr);

	s_expr r = s_list(2, form);

	for (int i = 0; i < size; i++) {
		s_dealias(form[i]);
	}

	return r;
}

s_function_info inspect_lambda(void* d) {
	s_bound_lambda* l = d;

	return (s_function_info){ l->lambda->param_count, l->lambda->body.term_count };
}

s_result apply_lambda(s_instruction* result, s_expr* args, void* d) {
	s_bound_lambda* l = d;
	;
}

void free_lambda(void* d) {
	s_bound_lambda* l = d;
	for (int i = 0; i < l->lambda->var_count; i++) {
		s_dealias(l->capture[i]);
	}
	free(l->capture);
	s_free_lambda(l->lambda);
}

static s_function_type lambda_function = {
	u"lambda",
	represent_lambda,
	inspect_lambda,
	apply_lambda,
	free_lambda
};

void eval_expression(s_expr* result, s_term e, const s_expr* args, const s_expr* closure) {
	switch (e.type) {
		case VARIABLE:
			switch (e.variable.type) {
				case PARAMETER:
					*result = args[e.variable.index];

				case CAPTURE:
					*result = closure[e.variable.index];

				default:
					assert(false);
			}

		case LAMBDA:
			;
			s_lambda l = {
			};
			*result = s_function(&lambda_function, sizeof(s_lambda), &l);

		default:
			*result = s_alias(e.quote);
	}
}

void eval_statement(instruction_slot* result, s_statement s, const s_expr* args, const s_expr* closure) {
	prepare_instruction_slot(result, s.term_count);

	for (int i = 0; i < s.term_count; i++) {
		eval_expression(&result->instruction.values[i], s.terms[i], args, closure);
	}
}

s_result execute_instruction(instruction_slot* next, instruction_slot* current) {
	if (current->instruction.values[0].type != FUNCTION) {
		return S_ATTEMPT_TO_CALL_NON_FUNCTION;
	}
	
	s_function_data* f = &current->instruction.values[0].p->function;

	s_function_info i = f->type->inspect(f + 1);

	prepare_instruction_slot(next, i.max_result_size);

	if (i.arg_count != current->instruction.size - 1) {
		return S_ARGUMENT_COUNT_MISMATCH;
	}

	return f->type->apply(&next->instruction, current->instruction.values, f + 1);
}

s_result s_eval(const s_statement s, const s_expr* args) {
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

	s_result r = S_SUCCESS;
	while (r == S_SUCCESS && current->instruction.size > 0) {
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

