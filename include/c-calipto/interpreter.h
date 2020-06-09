typedef enum s_variable_type {
	PARAMETER,
	CAPTURE
} s_variable_type;

typedef struct s_variable {
	s_variable_type type : 1;
	uint32_t index : 31;
} s_variable;

typedef enum s_term_type {
	LAMBDA = -1,
	VARIABLE = -2,
	// anything else is an s_expr_type and represents a QUOTE
} s_term_type;

struct s_lambda;

typedef union s_term {
	struct {
		s_term_type type;
		union {
			s_variable variable;
			struct s_lambda* lambda;
		};
	};
	s_expr quote;
} s_term;

typedef struct s_statement {
	uint32_t term_count;
	s_term* terms; // borrowed, always QUOTE | LAMBDA | VARIABLE
} s_statement;

typedef struct s_lambda {
	_Atomic(uint32_t) ref_count;
	uint32_t param_count;
	s_expr_ref** params; // always SYMBOL
	uint32_t var_count;
	uint32_t* vars; // indices into vars of lexical context
	s_statement body;
} s_lambda;

typedef struct s_bound_lambda {
	s_lambda* lambda;
	s_expr* capture;
} s_bound_lambda;

s_term s_alias_term(s_term t);
void s_dealias_term(s_term t);
s_lambda* s_ref_lambda(s_lambda* r);
void s_free_lambda(s_lambda* r);

s_result s_compile(s_statement* s, const s_expr e, const uint32_t param_count, const s_expr_ref** params);
s_result s_eval(const s_statement s, const s_expr* args);
