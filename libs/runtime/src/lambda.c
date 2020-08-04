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

ovru_lambda* ref_lambda(ovru_lambda* l) {
	atomic_fetch_add(&l->ref_count, 1);
	return l;
}

void free_lambda(ovru_lambda* l) {
	if (atomic_fetch_add(&l->ref_count, -1) > 1) {
		return;
	}
	ovs_dealias(l->params);
	if (l->capture_count > 0) {
		free(l->captures);
	}
	for (int i = 0; i < l->body.term_count; i++) {
		ovru_dealias_term(l->body.terms[i]);
	}
	free(l);
}

ovru_term ovru_alias_term(ovru_term t) {
	if (t.type == OVRU_LAMBDA) {
		ref_lambda(t.lambda);
	}
	return t;
}

void ovru_dealias_term(ovru_term t) {
	if (t.type == OVRU_LAMBDA) {
		free_lambda(t.lambda);
	}
}

typedef struct bound_lambda_data {
	ovru_lambda* lambda;
	ovs_expr* closure;
} bound_lambda_data;

ovs_expr bound_lambda_represent(const ovs_function_data* d) {
	const bound_lambda_data* l = (bound_lambda_data*)(d + 1);

	ovs_table* t = d->context->root_tables + OVS_DATA_LAMBDA;
	ovs_expr form[] = {
		l->lambda->params,
		ovs_list(t, 0, NULL) // TODO
	};
	uint32_t size = sizeof(form) / sizeof(ovs_expr);

	ovs_expr r = ovs_list(t, 2, form);

	for (int i = 0; i < size; i++) {
		ovs_dealias(form[i]);
	}

	return r;
}

ovs_function_info bound_lambda_inspect(const ovs_function_data* d) {
	const bound_lambda_data* l = ovs_function_extra_data(d);

	return (ovs_function_info){ l->lambda->param_count, l->lambda->body.term_count };
}

int32_t bound_lambda_apply(ovs_instruction* result, ovs_expr* args, const ovs_function_data* d);

void bound_lambda_free(const void* d) {
	const bound_lambda_data* l = d;
	for (int i = 0; i < l->lambda->capture_count; i++) {
		ovs_dealias(l->closure[i]);
	}
	free(l->closure);
	free_lambda(l->lambda);
}

static ovs_function_type bound_lambda_function = {
	u"lambda",
	bound_lambda_represent,
	bound_lambda_inspect,
	bound_lambda_apply,
	bound_lambda_free
};

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

ovs_expr ovru_bind_lambda(ovs_context* c, ovru_lambda* l) {
		bound_lambda_data* b;
		ovs_expr f = ovs_function(c, &bound_lambda_function, sizeof(bound_lambda_data), (void**)&l);
		*b = (bound_lambda_data){ ref_lambda(l), NULL };
		return f;
}

