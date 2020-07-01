typedef enum ovru_result {
	OVRU_SUCCESS,
	OVRU_ATTEMPT_TO_CALL_NON_FUNCTION,
	OVRU_ARGUMENT_COUNT_MISMATCH,
	OVRU_EMPTY_STATEMENT,
	OVRU_INVALID_STATEMENT_TERMINATOR,
	OVRU_EMPTY_EXPRESSION,
	OVRU_INVALID_EXPRESSION_TERMINATOR,
	OVRU_INVALID_EXPRESSION_TYPE,
	OVRU_INVALID_QUOTE_LENGTH,
	OVRU_INVALID_LAMBDA_LENGTH,
	OVRU_INVALID_PARAMETER_TERMINATOR,
	OVRU_VARIABLE_NOT_IN_SCOPE
} ovru_result;

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
	OVRU_VARIABLE = -2,
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
	ovs_expr_ref** params; // always SYMBOL
	uint32_t capture_count;
	ovru_variable* captures; // indices into vars of lexical context
	ovru_statement body;
} ovru_lambda;

typedef struct ovru_bound_lambda {
	ovru_lambda* lambda;
	ovs_expr* capture;
} ovru_bound_lambda;

ovru_term ovru_alias_term(ovru_term t);
void ovru_dealias_term(ovru_term t);
ovru_lambda* ovru_ref_lambda(ovru_lambda* r);
void ovru_free_lambda(ovru_lambda* r);

ovru_result ovru_compile(ovru_statement* s, const ovs_expr e, const uint32_t param_count, const ovs_expr_ref** params);
ovru_result ovru_eval(const ovru_statement s, const ovs_expr* args);
