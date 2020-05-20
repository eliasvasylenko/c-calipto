struct s_symbol_data;
struct s_cons_data;
struct s_lambda_data;
struct s_function_data;
struct s_builtin_data;
struct s_statement_data;
struct s_instruction_data;

typedef enum s_expr_type {
	ERROR,
	SYMBOL,
	CONS,
	NIL,
	QUOTE,
	LAMBDA,
	FUNCTION,
	BUILTIN,
	STATEMENT,
	INSTRUCTION, // a statement with all vars bound
	CHARACTER,
	STRING,
	INTEGER,
	BIG_INTEGER
} s_expr_type;

typedef struct s_expr {
	s_expr_type type;
	_Atomic(int32_t)* ref_count;
	union {
		UChar* error;
		struct s_symbol_data* symbol;
		struct s_cons_data* cons;
		void* nil;
		struct s_expr* quote;
		struct s_lambda_data* lambda;
		struct s_function_data* function;
		struct s_builtin_data* builtin;
		struct s_statement_data* statement;
		struct s_instruction_data* instruction;
		UChar32 character;
		UChar* string;
		int64_t integer;
	};
} s_expr;

typedef struct s_symbol_data {
	UChar* name;
	s_expr* qualifier;
} s_symbol_data;

typedef struct s_cons_data {
	s_expr car;
	s_expr cdr;
} s_cons_data;


/*
 * An expression can be promoted to a lambda when it
 * appears as a term in a statement. This promotion
 * does not modify the data, it is merely a
 * specialization for the purpose of performance.
 */
typedef struct s_lambda_data {
	int32_t param_count;
	s_expr* params;
	s_expr body;
} s_lambda_data;

typedef struct s_statement_data {
	int32_t free_var_count;
	s_expr* free_vars;
	s_expr target;
	int32_t arg_count;
	s_expr* args;
} s_statement_data;

typedef struct s_function_data {
	s_expr lambda;
	s_expr* capture;
} s_function_data;

typedef struct s_builtin_data {
	UChar* name;
	int32_t arg_count;
	bool (*apply)(s_expr* result, s_expr* args, void* d);
	void (*free) (void* data);
	void* data;
} s_builtin_data;

typedef struct s_instruction_data {
	s_expr statement;
	s_expr* bindings;
} s_instruction_data;

s_expr s_nil();
s_expr s_symbol(strref ns, strref n);
s_expr s_cons(s_expr car, s_expr cdr);
s_expr s_character(UChar32 c);
s_expr s_string(strref s);
s_expr s_builtin(strref n, int32_t c,
		bool (*a)(s_expr* result, s_expr* a, void* d),
		void (*f)(void* d),
		void* data);
s_expr s_quote(s_expr data);
s_expr s_lambda(int32_t param_count, s_expr* params,
		s_expr body);
s_expr s_statement(int32_t free_var_count, s_expr* free_vars,
		s_expr target,
		int32_t arg_count, s_expr* args);

s_expr s_function(s_expr lambda, s_expr* capture);
s_expr s_error(strref message);

UChar* s_name(s_expr s);
UChar* s_namespace(s_expr s);

s_expr s_car(s_expr s);
s_expr s_cdr(s_expr s);
bool s_atom(s_expr e);
bool s_eq(s_expr a, s_expr b);

void s_ref(s_expr s);
void s_free(s_expr s);
void s_dump(s_expr s);

/*
 * This is a trie mapping symbol names & qualifiers to symbols, used
 * for interning. Keys are composed of a fixed-length pointer and a
 * variable-length string, which can be viewed as a variable-length
 * sequence of bytes. Pretty well understood any easy to optimise.
 *
 * The fixed-length qualifiers will make for many common prefixes, so
 * we should optimise for this. Perhaps a two-level data structure
 * where we map by qualifier first then switch to a different strategy
 * for subtrees?
 *
 * Since symbols are reference counted, they should remove themselves
 * from this trie once they're free.
 */
typedef struct s_table {
	// mutex?
	union s_table_node {
		struct {
			int64_t offset; // for common prefix compression, negated
			uint64_t population[4]; // for popcount compression
			union s_table_node* children;
		}* internal;
		struct {
			s_expr value;
			s_expr* key_qualifier;
			UChar key_name[1]; // variable length, null terminated
		}* leaf;
	} root;
} s_table;

/*
 * Associative trie for binding symbols to values. All keys are known before-hand,
 * so we can allocate every node into one flat block of memory. We only need to
 * support update and retrieval of keys which are present. Keys are pointers to
 * interned symbols.
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

typedef struct s_bound_expr {
	s_expr expr;
	s_bindings bindings;
} s_bound_expr;

