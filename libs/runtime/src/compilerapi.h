/*
 * Compiler State
 */

typedef struct variable_capture {
	const ovs_expr_ref* symbol;
	ovru_variable variable;
	uint32_t depth;
} variable_capture;

typedef struct compile_state {
	_Atomic(int32_t) counter;
	struct compile_state* parent;
	ovs_context* context;

	bdtrie variables;
	uint32_t capture_count;
	variable_capture* captures;
	uint32_t param_count;
	ovs_expr params;

	uint32_t propagated_capture_count;
	uint32_t total_capture_count;

	ovru_statement body;
	const ovs_expr_ref* cont;
} compile_state;

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

/*
 * Statement Function
 */

typedef struct statement_data {
	compile_state* state;
} statement_data;

ovs_expr statement_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

void statement_free(const void* d) {
	statement_data* data = *(statement_data**)d;

	free_compile_state(data->state);
	free(data);
}

ovs_function_info statement_inspect(const ovs_function_data* d);

int32_t statement_with_lambda_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t statement_with_variable_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t statement_with_quote_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t statement_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);

static ovs_function_type statement_with_lambda_function = {
	u"statement-with-lambda",
	statement_represent,
	statement_inspect,
	statement_with_lambda_apply,
	statement_free
};

static ovs_function_type statement_with_variable_function = {
	u"statement-with-variable",
	statement_represent,
	statement_inspect,
	statement_with_variable_apply,
	statement_free
};

static ovs_function_type statement_with_quote_function = {
	u"statement-with-quote",
	statement_represent,
	statement_inspect,
	statement_with_quote_apply,
	statement_free
};

static ovs_function_type statement_end_function = {
	u"statement-end",
	statement_represent,
	statement_inspect,
	statement_end_apply,
	statement_free
};

ovs_function_info statement_inspect(const ovs_function_data* d) {
	if (d->type == &statement_with_lambda_function)
		return (ovs_function_info){ 3, 2 };

	else if (d->type == &statement_with_lambda_function)
		return (ovs_function_info){ 2, 2 };

	else if (d->type == &statement_with_quote_function)
		return (ovs_function_info){ 2, 2 };

	else if (d->type == &statement_end_function)
		return (ovs_function_info){ 1, 2 };

	assert(false);
}

/*
 * Parameters Function
 */

typedef struct parameters_data {
	compile_state* state;
	ovs_expr params;
	const ovs_expr_ref* cont;
} parameters_data;

ovs_expr parameters_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

void parameters_free(const void* d) {
	parameters_data* data = *(parameters_data**)d;

	free_compile_state(data->state);
	free(data);
}

ovs_function_info parameters_inspect(const ovs_function_data* d);

int32_t parameters_with_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t parameters_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);

static ovs_function_type parameters_with_function = {
	u"parameters-with",
	parameters_represent,
	parameters_inspect,
	parameters_with_apply,
	parameters_free
};

static ovs_function_type parameters_end_function = {
	u"parameters-end",
	parameters_represent,
	parameters_inspect,
	parameters_end_apply,
	parameters_free
};

ovs_function_info parameters_inspect(const ovs_function_data* d) {
	if (d->type == &parameters_with_function)
		return (ovs_function_info){ 2, 3 };

	else if (d->type == &parameters_end_function)
		return (ovs_function_info){ 1, 3 };

	assert(false);
}

/*
 * Compile
 */

ovs_expr compile_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

ovs_function_info compile_inspect(const ovs_function_data* d) {
	return (ovs_function_info){ 3, 3 };
}

void compile_free(const void* d) {}

int32_t compile_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);

static ovs_function_type compile_function = {
	u"compile",
	compile_represent,
	compile_inspect,
	compile_apply,
	compile_free
};

