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
#include "c-ohvu/runtime/evaluator.h"

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

ovs_expr represent_param(const void* p) {
	return (ovs_expr){ OVS_SYMBOL, .p=*(ovs_expr_ref**)p };
}

ovs_expr bound_lambda_represent(const ovs_function_data* d) {
	const ovru_bound_lambda* l = (ovru_bound_lambda*)(d + 1);

	ovs_table* t = d->context->root_tables + OVS_DATA_LAMBDA;
	ovs_expr form[] = {
		ovs_list_of(t, l->lambda->param_count, (void**)l->lambda->params, represent_param),
		ovs_list(t, 0, NULL) // TODO
	};
	uint32_t size = sizeof(form) / sizeof(ovs_expr);

	ovs_expr r = ovs_list(t, 2, form);

	for (int i = 0; i < size; i++) {
		ovs_dealias(form[i]);
	}

	return r;
}

ovs_function_info bound_lambda_inspect(const void* d) {
	const ovru_bound_lambda* l = d;

	return (ovs_function_info){ l->lambda->param_count, l->lambda->body.term_count };
}

void eval_statement(ovs_context* c, ovs_instruction* result, ovru_statement s, const ovs_expr* args, const ovs_expr* closure);

int32_t bound_lambda_apply(ovs_instruction* result, ovs_expr* args, const ovs_function_data* d) {
	const ovru_bound_lambda* l = (ovru_bound_lambda*)(d + 1);

	eval_statement(d->context, result, l->lambda->body, args + 1, l->closure);

	return OVRU_SUCCESS;
}

void bound_lambda_free(const void* d) {
	const ovru_bound_lambda* l = d;
	for (int i = 0; i < l->lambda->capture_count; i++) {
		ovs_dealias(l->closure[i]);
	}
	free(l->closure);
	free_lambda(l->lambda);
}

static ovs_function_type lambda_function = {
	u"lambda",
	bound_lambda_represent,
	bound_lambda_inspect,
	bound_lambda_apply,
	bound_lambda_free
};

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
			*result = ovs_function(context, &lambda_function, sizeof(ovru_bound_lambda), &l);
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

ovru_result execute_instruction(instruction_slot* next, instruction_slot* current) {
	if (current->instruction.values[0].type != OVS_FUNCTION) {
		printf("\nAttempt To Call Non Function\n  ");
		ovs_dump_expr(current->instruction.values[0]);
		return OVRU_ATTEMPT_TO_CALL_NON_FUNCTION;
	}
	
	const ovs_function_data* f = &current->instruction.values[0].p->function;

	ovs_function_info i = f->type->inspect(f + 1);

	prepare_instruction_slot(next, i.max_result_size);

	if (i.arg_count != current->instruction.size - 1) {
		printf("\nArgument Count Mismatch\n  ");
		ovs_dump_expr(current->instruction.values[0]);
		return OVRU_ARGUMENT_COUNT_MISMATCH;
	}

	ovru_result r = f->type->apply(&next->instruction, current->instruction.values, f);

	clear_instruction_slot(current);

	return r;
}

ovru_result ovru_eval(const ovru_statement s, ovs_context* c, const ovs_expr* args) {
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
	eval_statement(c, &current->instruction, s, args, NULL);

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

