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

#define ARGS_ON_STACK 16
#define ARGS_ON_HEAP 32

/*
 * Evaluator
 */

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

ovru_result execute_instruction(instruction_slot* next, instruction_slot* current) {
	if (current->instruction.values[0].type != OVS_FUNCTION) {
		printf("\nAttempt To Call Non Function\n  ");
		ovs_dump_expr(current->instruction.values[0]);
		return OVRU_INVALID_CALL_TARGET;
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

ovru_result ovru_eval(ovs_instruction ins) {
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
	prepare_instruction_slot(current, ins.size);
	current->instruction.size = ins.size;
	for (int i = 0; i < ins.size; i++) {
		current->instruction.values[i] = ovs_alias(ins.values[i]);
	}

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

