/*
 * Statement Function
 */

typedef struct statement_data {
	_Atomic(int32_t) counter;
	compile_context* context;
	ovru_statement statement;
	const ovs_expr_ref* parent;
} statement_data;

ovs_expr statement_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

void statement_free(const void* d) {
	statement_data* data = (statement_data*)d;

	ovs_free(OVS_FUNCTION, data->data->parent);
	free(data->data);
}

ovs_function_info statement_inspect(ovs_function_data* d) {
	assert(false);
}

int32_t statement_with_lambda_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t statement_with_variable_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t statement_with_quote_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t statement_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);

static ovs_function_type statement_function = {
	u"statement",
	statement_represent,
	statement_inspect,
	statement_apply,
	statement_free
};

ovs_function_info statement_inspect(ovs_function_data* d) {
	switch (d->type) {
		case &statement_with_lambda_function:
			return (ovs_function_info){ 3, 2 };
		case &statement_with_lambda_function:
			return (ovs_variable_info){ 2, 2 };
		case &statement_with_quote_function:
			return (ovs_function_info){ 2, 2 };
		case &statement_end_function:
			return (ovs_function_info){ 1, 2 };
	}

/*
 * Parameters Function
 */

typedef struct parameters_data {
	_Atomic(int32_t) counter;
	ovs_expr params;
	const ovs_expr_ref* statement_cont;
	const ovs_expr_ref* parent;
} parameters_data;

ovs_expr parameters_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

void parameters_free(const void* d) {
	parameters_data* data = (parameters_data*)d;

	ovs_free(OVS_FUNCTION, data->data->parent);
	free(data->data);
}

ovs_function_info parameters_inspect(const void* d) {
	switch (((parameters_data*)d)->type) {
		case PARAMETERS_WITH:
			return (ovs_function_info){ 2, 3 };
		case PARAMETERS_END:
			return (ovs_function_info){ 1, 3 };
	}
	assert(false);
}

int32_t parameters_with_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t parameters_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);

static ovs_function_type parameters_function = {
	u"parameters",
	parameters_represent,
	parameters_inspect,
	parameters_apply,
	parameters_free
};

/*
 * Compile
 */

ovs_expr compile_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

ovs_function_info compile_inspect(const void* d) {
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

