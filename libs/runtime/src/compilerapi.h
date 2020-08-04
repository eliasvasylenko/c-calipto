/*
 * Lambda
 */

typedef enum ovru_variable_type {
	OVRU_PARAMETER,
	OVRU_CAPTURE
} ovru_variable_type;

typedef struct ovru_variable {
	ovru_variable_type type : 1;
	uint32_t index : 31;
} ovru_variable;

typedef enum ovru_term_type {
	OVRU_LAMBDA = -1,
	OVRU_VARIABLE = -2
	// anything else is an ovs_expr_type and represents a QUOTE
} ovru_term_type;

struct ovru_lambda;

typedef union ovru_term {
	struct {
		ovru_term_type type;
		union {
			ovru_variable variable;
			struct ovru_lambda* lambda;
		};
	};
	ovs_expr quote;
} ovru_term;

typedef struct ovru_statement {
	uint32_t term_count;
	ovru_term* terms; // borrowed, always QUOTE | LAMBDA | VARIABLE
} ovru_statement;

typedef struct ovru_lambda {
	_Atomic(uint32_t) ref_count;
	uint32_t param_count;
	ovs_expr params; // as linked list
	uint32_t capture_count;
	ovru_variable* captures; // indices into vars of lexical context
	ovru_statement body;
} ovru_lambda;

ovru_term ovru_alias_term(ovru_term t);
void ovru_dealias_term(ovru_term t);

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

compile_state* make_compile_state(compile_state* parent, ovs_context* oc, const ovs_expr_ref* cont);

compile_state* ref_compile_state(compile_state* s);

void free_compile_state(compile_state* c);

bool is_unique_compile_state(compile_state* s);

uint32_t find_variable(ovru_variable* result, compile_state* c, const ovs_expr_ref* symbol);

/*
 * Statement Function
 */

ovs_expr statement_function(compile_state* s, ovs_function_type* t);

/*
 * Parameters Function
 */

ovs_expr parameters_function(compile_state* s, ovs_expr params, const ovs_expr_ref* cont, ovs_function_type* t);

/*
 * Compile
 */

