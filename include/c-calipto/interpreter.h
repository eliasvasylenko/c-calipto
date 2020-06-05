// terms

typedef enum s_term_type {
	LAMBDA = -1,
	VARIABLE = -2
	// anything else is an s_expr_type and represents a QUOTE
} s_term_type;

struct s_lambda_term;

typedef union s_term {
	struct {
		s_term_type type;
		union {
			uint32_t variable;
			struct s_lambda_term* lambda;
		};
	};
	s_expr quote;
} s_term;

// statements

typedef struct s_statement {
	int32_t term_count;
	s_term* terms; // borrowed, always QUOTE | LAMBDA | VARIABLE
	s_expr* bindings; // owned
} s_statement;

typedef struct s_lambda_term {
	_Atomic(uint32_t) ref_count;
	uint32_t param_count;
	s_expr_ref** params; // always SYMBOL
	uint32_t var_count;
	uint32_t* vars; // indices into vars of lexical context
	uint32_t term_count;
	s_term* terms;
} s_lambda_term;

/*
 * Associative trie for binding symbols to indices. This is how the array of
 * variables captured by a lambda is populated from the enclosing scope.
 *
 * All keys are known before-hand, so we can allocate every node into one flat
 * block of memory. We only need to support update and retrieval of keys which
 * are present. Keys are pointers to interned symbols.
 *
 * Node arrays use popcnt compression, and the structure uses prefix compression.
 *
 * We never need to synchronize access.
 */
typedef struct s_bindings {
	uint32_t node_count; // only needed for fast copying

	/*
	 * Tree from symbols to expression.
	 *
	 * Since symbols are interned, we can distinguish them uniquely by pointer.
	 *
	 * This means we have a unique 32/64 bit key ready to go, so perhaps a clever
	 * trie implementation can have good results and be simple.
	 *
	 * For some tables (e.g. captured bindings) we have a small, fixed set of keys
	 * and want to make many copies of the table with different values. To achieve
	 * this perhaps we can find a perfect hash for a set of keys? Or just make a
	 * best-effort to find an offset into the pointer data which already produces
	 * a unique byte/nibble for the pointer data. In those cases we might also be
	 * able to assume that all lookups will succeed in which case can omit key.
	 *
	 * Can also try popcount compression on resulting array.
	 */
	struct s_bindings_node {
		bool leaf;
		union {
			struct {
				uint8_t offset;
				uint16_t population;
				struct s_bindings_node* children;
			};
			s_expr value;
		};
	}* nodes;
} s_bindings;

typedef struct s_bound_arguments {
	s_bindings bindings;
	s_expr* values;
} s_bound_arguments;

s_bindings s_prepare_bindings(s_expr_ref** symbols);
s_bindings s_free_bindings(s_bindings);

s_bound_arguments s_bind_arguments(s_expr_ref** symbols, s_expr* v);
void s_unbind_arguments(s_bound_arguments b);

s_statement s_compile(const s_expr e, const uint32_t param_count, const s_expr_ref** params);

void s_eval(const s_statement s, const s_expr* args);

s_term s_quote(s_expr data);
s_term s_lambda(uint32_t param_count, s_expr_ref** params,
		uint32_t var_count, uint32_t* vars,
		uint32_t term_count, s_term* terms);
s_term s_variable(uint64_t offset);

s_term s_alias_term(s_term t);
void s_dealias_term(s_term t);
s_lambda_term* s_ref_lambda(s_lambda_term* r);
void s_free_lambda(s_lambda_term* r);

