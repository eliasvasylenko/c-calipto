/*
 * Compiler State
 */

typedef struct variable_capture {
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

	ovru_statement body;
	const ovs_expr_ref* cont;
} compile_state;

void compile_state_free(compile_state* c) {
	if (atomic_fetch_add(&c->counter, -1) == 1) {
		compile_state_free(c->parent);

		if (c->capture_count > 0)
			free(c->captures);
		ovs_dealias(c->params);
		bdtrie_clear(&c->variables);

		if (c->body.term_count > 0)
			free(c->body.terms);
		
		ovs_free(OVS_FUNCTION, c->cont);
	}
}

/*
 * Find the variable in the current lexical scope by traversing down the stack of enclosing
 * lambdas.
 *
 * If the variable was found, returns its depth and sets `result` to the variable's location
 * at that depth.
 *
 * If the variable was not found, returns -1;
 */
uint32_t find_variable(ovru_variable* result, compile_state* c, const ovs_expr_ref* symbol) {
	bdtrie_value v = bdtrie_find(&c->variables, sizeof(ovs_expr_ref*), &symbol);

	if (bdtrie_is_present(v)) {
		*result = *(ovru_variable*)v.data;
		return true;

	} else {
		if (c->parent == NULL) {
			return -1;
		}
		int32_t r = find_variable(result, c->parent, symbol);
		return (r < 0) ? r : r + 1;
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

	compile_state_free(data->state);
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
	compile_state* enclosing_state;
	ovs_expr params;
	const ovs_expr_ref* statement_cont;
} parameters_data;

ovs_expr parameters_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

void parameters_free(const void* d) {
	parameters_data* data = *(parameters_data**)d;

	compile_state_free(data->enclosing_state);
	ovs_free(OVS_FUNCTION, data->statement_cont);
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

