/*
 * Types
 */

// expressions

typedef enum s_expr_type {
	ERROR,
	SYMBOL,
	CONS,

	FUNCTION,
	BUILTIN,

	CHARACTER,
	STRING,
	INTEGER,
	BIG_INTEGER
} s_expr_type;


typedef struct s_expr_ref s_expr_ref;

typedef struct s_expr {
	s_expr_type type;
	union {
		UChar32 character;
		int64_t integer;
		s_expr_ref* p;
	};
} s_expr;

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

/*
 * Data
 */

typedef struct s_lambda_term {
	_Atomic(uint32_t) ref_count;
	uint32_t param_count;
	s_expr_ref** params; // always SYMBOL
	uint32_t var_count;
	uint32_t* vars; // indices into vars of lexical context
	uint32_t term_count;
	s_term* terms;
} s_lambda_term;

typedef struct s_table {
	idtrie trie;
} s_table;

typedef struct s_symbol_info {
	s_expr qualifier;
	UChar name[1];
} s_symbol_info;

typedef struct s_cons_data {
	s_expr car;
	s_expr cdr;
} s_cons_data;

typedef struct s_function_data {
	s_lambda_term* lambda;
	s_expr* capture;
} s_function_data;

typedef struct s_builtin_data {
	s_expr_ref* name; // always SYMBOL
	s_expr (*represent)(void* data);
	int32_t arg_count;
	bool (*apply)(s_statement* result, s_expr* args, void* d);
	void (*free) (void* data);
	void* data;
} s_builtin_data;

typedef struct s_string_data {
	UChar string[1]; // variable length
} s_string_data;

struct s_expr_ref {
	_Atomic(uint32_t) ref_count;
	union {
		id symbol;
		s_cons_data cons;
		s_function_data function;
		s_builtin_data builtin;
		s_string_data string;
	};
};

/*
 * Functions
 */

void s_init();
void s_close();

s_expr s_symbol(s_expr_ref* qualifier, strref name);
s_expr s_cons(s_expr car, s_expr cdr);

s_expr s_list(int32_t count, s_expr* e);

s_expr s_character(UChar32 c);
s_expr s_string(strref s);
s_expr s_builtin(s_expr_ref* n,
		s_expr (*r)(void* d),
		int32_t c,
		bool (*a)(s_statement* result, s_expr* a, void* d),
		void (*f)(void* d),
		void* data);

s_expr s_function(s_lambda_term* lambda, s_expr* capture);
s_expr s_error();

s_symbol_info* s_inspect(s_expr s);
s_expr s_car(s_expr s);
s_expr s_cdr(s_expr s);

bool s_atom(s_expr e);
bool s_eq(s_expr a, s_expr b);

void s_dump(s_expr s);

s_expr s_alias(s_expr s);
void s_dealias(s_expr s);
s_expr_ref* s_ref(s_expr_ref* r);
void s_free(s_expr_type t, s_expr_ref* r);

s_term s_quote(s_expr data);
s_term s_lambda(uint32_t param_count, s_expr_ref** params,
		uint32_t var_count, uint32_t* vars,
		uint32_t term_count, s_term* terms);
s_term s_variable(uint64_t offset);

s_term s_alias_term(s_term t);
void s_dealias_term(s_term t);
s_lambda_term* s_ref_lambda(s_lambda_term* r);
void s_free_lambda(s_lambda_term* r);
