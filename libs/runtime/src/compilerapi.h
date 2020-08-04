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

ovs_expr ovru_bind_lambda(ovs_context* c, ovru_lambda* l);

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
void compile_state_with_parameters(compile_state* s, ovs_expr params);
void compile_state_without_parameters(compile_state* s);
void compile_state_with_captures(compile_state* s, uint32_t capture_count, variable_capture* captures);
void compile_state_with_term(compile_state* s, ovru_term t);

compile_state* ref_compile_state(compile_state* s);

void free_compile_state(compile_state* c);

bool is_unique_compile_state(compile_state* s);

uint32_t find_variable(ovru_variable* result, compile_state* c, const ovs_expr_ref* symbol);

/*
 * Statement Function
 */

typedef enum statement_function_type {
	STATEMENT_WITH_LAMBDA,
	STATEMENT_WITH_VARIABLE,
	STATEMENT_WITH_QUOTE,
	STATEMENT_END
} statement_function_type;

ovs_expr statement_function(compile_state* s, statement_function_type t);

/*
 * Parameters Function
 */

typedef enum parameters_function_type {
	PARAMETERS_WITH,
	PARAMETERS_END
} parameters_function_type;

ovs_expr parameters_function(compile_state* s, ovs_expr params, const ovs_expr_ref* cont, parameters_function_type t);

