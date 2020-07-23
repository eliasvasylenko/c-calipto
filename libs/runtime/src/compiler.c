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

typedef struct variable_bindings {
	uint32_t capture_count;
	uint32_t param_count;
	ovru_variable* captures;
	bdtrie variables;
} variable_bindings;

void* get_variable_binding(uint32_t key_size, const void* key_data, const void* value_data, bdtrie_node* owner) {
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
	ovs_context* ovs_context;
	variable_bindings bindings;
} compile_context;

ovru_result find_variable(ovru_variable* result, compile_context* c, const ovs_expr_ref* symbol) {
	ovru_variable v = { OVRU_CAPTURE, c->bindings.capture_count };
	ovru_variable w = *(ovru_variable*)bdtrie_find_or_insert(&c->bindings.variables, sizeof(ovs_expr_ref*), &symbol, &v).data;

	if (w.index == v.index && w.type == v.type) {
		if (c->parent == NULL) {
			ovs_expr e = { OVS_SYMBOL, .p=symbol };
			printf("\nVariable Not In Scope\n  ");
			ovs_dump_expr(e);
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

ovru_result compile_quote(ovru_term* result, int32_t part_count, ovs_expr* parts, compile_context* c, ovs_expr e) {
	if (part_count != 1) {
		printf("\nInvalid Quote Length\n  ");
		ovs_dump_expr(e);
		return OVRU_INVALID_QUOTE_LENGTH;
	}

	*result = (ovru_term){ .quote=parts[0] };

	return OVRU_SUCCESS;
}

ovru_result compile_statement(ovru_statement* result, ovs_expr s, compile_context* c);

void* get_ref(ovs_expr e) {
	return (void*)ovs_ref(e.p);
}

ovru_result compile_lambda(ovru_term* result, int32_t part_count, ovs_expr* parts, compile_context* c, ovs_expr e) {
	if (part_count != 2) {
		printf("\nInvalid Lambda Length\n  ");
		ovs_dump_expr(e);
		return OVRU_INVALID_LAMBDA_LENGTH;
	}

	ovs_expr params_decl = parts[0];
	ovs_expr body_decl = parts[1];

	ovs_expr_ref** params;
	int32_t param_count = ovs_delist_of(c->ovs_context->root_tables, params_decl, (void***)&params, get_ref);
	if (param_count < 0) {
		printf("\nInvalid Parameter Terminator\n  ");
		ovs_dump_expr(e);
		return OVRU_INVALID_PARAMETER_TERMINATOR;
	}

	compile_context lambda_context = {
		c,
		c->ovs_context,
		make_variable_bindings(param_count, (const ovs_expr_ref**) params)
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
	if (ovs_is_symbol(e)) {
		ovru_variable v;
		ovru_result r = find_variable(&v, c, e.p);
		if (r == OVRU_SUCCESS) {
			*result = (ovru_term){ .type=OVRU_VARIABLE, .variable=v };
		}
		return r;
	}

	ovs_expr* parts;
	uint32_t count = ovs_delist(c->ovs_context->root_tables + OVS_UNQUALIFIED, e, &parts);
	if (count <= 0) {
		if (count == 0) {
			printf("\nEmpty Expression\n  ");
			ovs_dump_expr(e);
			return OVRU_EMPTY_EXPRESSION;
		} else {
			printf("\nInvalid Expression Terminator\n  ");
			ovs_dump_expr(e);
			return OVRU_INVALID_EXPRESSION_TERMINATOR;
		}
	}

	ovs_expr kind = parts[0];
	int32_t term_count = count - 1;
	ovs_expr* terms = parts + 1;

	ovru_result success;
	if (ovs_is_eq(kind, ovs_root_symbol(OVS_DATA_QUOTE)->expr)) {
		success = compile_quote(result, term_count, terms, c, e);

	} else if (ovs_is_eq(kind, ovs_root_symbol(OVS_DATA_LAMBDA)->expr)) {
		success = compile_lambda(result, term_count, terms, c, e);

	} else {
		printf("\nInvalid Expression Type\n  ");
		ovs_dump_expr(e);
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
	int32_t count = ovs_delist(c->ovs_context->root_tables + OVS_UNQUALIFIED, s, &expressions);
	if (count <= 0) {
		if (count == 0) {
			printf("\nEmpty Statement\n  ");
			ovs_dump_expr(s);
			return OVRU_EMPTY_STATEMENT;
		} else {
			printf("\nInvalid Statement Terminator\n  ");
			ovs_dump_expr(s);
			return OVRU_INVALID_STATEMENT_TERMINATOR;
		}
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

void compile(compile_context* result, ovs_context* c, const uint32_t param_count, const ovs_expr_ref** params) {
	*result = (compile_context){
		NULL,
		c,
		make_variable_bindings(param_count, params)
	};
}

ovru_result ovru_compile(ovru_statement* result, ovs_context* c, const ovs_expr e, const uint32_t param_count, const ovs_expr_ref** params) {
	compile_context context;
	compile(&context, c, param_count, params);

	ovru_result success = compile_statement(result, e, &context);

	if (context.bindings.capture_count > 0) {
		free(context.bindings.captures);
	}
	bdtrie_clear(&context.bindings.variables);

	return success;
}

/*
 * With Variable
 */

typedef struct with_variable_data {
	compile_context context;
	ovs_expr_ref* parent;
} with_variable_data;

ovs_expr with_variable_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

ovs_function_info with_variable_inspect(const void* d) {
	return (ovs_function_info){ 2, 2 };
}

int32_t with_variable_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d) {
	ovs_expr params = args[0];
	ovs_expr build = args[1];
	ovs_expr cont = args[2];

	return 1234321;
}

void with_variable_free(const void* d) {
	with_variable_data* data = (with_variable_data*)d;

	ovs_free(OVS_FUNCTION, data->parent);
}

static ovs_function_type with_variable_function = {
	u"with-variable",
	with_variable_represent,
	with_variable_inspect,
	with_variable_apply,
	with_variable_free
};

/*
 * With Quote
 */

typedef struct with_quote_data {
	compile_context context;
	ovs_expr_ref* parent;
} with_quote_data;

ovs_expr with_quote_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

ovs_function_info with_quote_inspect(const void* d) {
	return (ovs_function_info){ 3, 2 };
}

int32_t with_quote_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d) {
	ovs_expr params = args[0];
	ovs_expr build = args[1];
	ovs_expr cont = args[2];

	return 1234321;
}

void with_quote_free(const void* d) {
	with_quote_data* data = (with_quote_data*)d;

	ovs_free(OVS_FUNCTION, data->parent);
}

static ovs_function_type with_quote_function = {
	u"with-quote",
	with_quote_represent,
	with_quote_inspect,
	with_quote_apply,
	with_quote_free
};

/*
 * With Lambda
 */

typedef struct with_lambda_data {
	compile_context context;
	ovs_expr_ref* parent;
} with_lambda_data;

ovs_expr with_lambda_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

ovs_function_info with_lambda_inspect(const void* d) {
	return (ovs_function_info){ 3, 2 };
}

int32_t with_lambda_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d) {
	ovs_expr params = args[0];
	ovs_expr build = args[1];
	ovs_expr cont = args[2];

	return 1234321;
}

void with_lambda_free(const void* d) {
	with_lambda_data* data = (with_lambda_data*)d;

	ovs_free(OVS_FUNCTION, data->parent);
}

static ovs_function_type with_lambda_function = {
	u"with-lambda",
	with_lambda_represent,
	with_lambda_inspect,
	with_lambda_apply,
	with_lambda_free
};

/*
 * Compile
 */

typedef struct compile_data {
	compile_context context;
	ovs_expr cont;
} compile_data;

ovs_expr compile_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

ovs_function_info compile_inspect(const void* d) {
	return (ovs_function_info){ 3, 2 };
}

int32_t compile_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d) {
	ovs_expr params = args[0];
	ovs_expr build = args[1];
	ovs_expr cont = args[2];

	i->size = 2;

	return 1234321;
}

void compile_free(const void* d) {}

static ovs_function_type compile_function = {
	u"compile",
	compile_represent,
	compile_inspect,
	compile_apply,
	compile_free
};

