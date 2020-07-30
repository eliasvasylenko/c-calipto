/*
 * Compiler State
 */

typedef struct variable_bindings {
	uint32_t capture_count;
	ovru_variable* captures;
	uint32_t param_count;
	ovs_expr params;
	bdtrie variables;
} variable_bindings;

typedef struct compile_context {
	_Atomic(int32_t) counter;
	struct compile_context* parent;
	ovs_context* ovs_context;
	variable_bindings bindings;
} compile_context;

void compile_context_free(compile_context* c) {
	if (atomic_fetch_add(&c->counter, -1) == 1) {
		compile_context_free(c->parent);
		free(c->bindings.captures);
		ovs_dealias(c->bindings.params);
		bdtrie_clear(&c->bindings.variables);
	}
}

/*
 * Statement Function
 */

typedef struct statement_data {
	compile_context* context;
	ovru_statement statement;
	const ovs_expr_ref* cont;
} statement_data;

ovs_expr statement_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

void statement_free(const void* d) {
	statement_data* data = *(statement_data**)d;

	compile_context_free(data->context);
	ovs_free(OVS_FUNCTION, data->cont);
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
	compile_context* context;
	ovs_expr params;
	const ovs_expr_ref* statement_cont;
	const ovs_expr_ref* cont;
} parameters_data;

ovs_expr parameters_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

void parameters_free(const void* d) {
	parameters_data* data = *(parameters_data**)d;

	compile_context_free(data->context);
	ovs_free(OVS_FUNCTION, data->statement_cont);
	ovs_free(OVS_FUNCTION, data->cont);
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

