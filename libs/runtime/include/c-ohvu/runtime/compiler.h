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

ovs_expr ovru_bind_lambda(ovru_term l, ovs_expr* closure);

ovs_expr ovru_compile();

ovru_term ovru_alias_term(ovru_term t);
void ovru_dealias_term(ovru_term t);

