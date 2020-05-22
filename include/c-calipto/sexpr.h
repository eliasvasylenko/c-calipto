typedef enum s_expr_type {
	ERROR,
	SYMBOL,
	CONS,

	QUOTE,
	LAMBDA,
	VARIABLE,

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
		uint64_t variable;
		s_expr_ref* p;
	};
} s_expr;

typedef struct s_statement {
	int32_t term_count;
	s_expr* terms; // borrowed, always QUOTE | LAMBDA | VARIABLE
	s_expr* bindings; // owned
} s_statement;

typedef struct s_symbol_data {
	s_expr_ref* qualifier; // always SYMBOL
	uint32_t name_length;
	UChar name[1]; // variable length
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
	int32_t param_count; // <= var_count
	int32_t var_count;
	s_expr_ref** vars; // always SYMBOLs
	int32_t term_count;
	s_expr* terms; // always QUOTE | LAMBDA | VARIABLE
} s_lambda_data;

typedef struct s_function_data {
	s_expr_ref* lambda; // always LAMBDA
	s_expr* capture;
} s_function_data;

typedef struct s_builtin_data {
	s_expr_ref* name; // always SYMBOL
	s_expr (*represent)(void* data);
	int32_t arg_count;
	bool (*apply)(s_expr_ref** result, s_expr* args, void* d);
	void (*free) (void* data);
	void* data;
} s_builtin_data;

typedef struct s_string_data {
	UChar string[1]; // variable length
} s_string_data;

struct s_expr_ref {
	_Atomic(int32_t) ref_count;
	union {
		struct s_symbol_data symbol;
		struct s_cons_data cons;
		struct s_expr quote;
		struct s_lambda_data lambda;
		struct s_function_data function;
		struct s_builtin_data builtin;
		struct s_string_data string;
	};
};

s_expr s_nil();
s_expr s_symbol(s_expr_ref* qualifier, strref name);
s_expr s_cons(s_expr car, s_expr cdr);
s_expr s_character(UChar32 c);
s_expr s_string(strref s);
s_expr s_builtin(s_expr_ref* n,
		s_expr (*r)(void* d),
		int32_t c,
		bool (*a)(s_statement* result, s_expr* a, void* d),
		void (*f)(void* d),
		void* data);
s_expr s_quote(s_expr data);
s_expr s_lambda(int32_t param_count, s_expr_ref** params,
		s_expr_ref* body);
s_expr s_variable(uint64_t offset);

s_expr s_function(s_expr_ref* lambda, s_expr* capture);
s_expr s_error();

UChar* s_name(s_expr s);
UChar* s_namespace(s_expr s);

s_expr s_car(s_expr s);
s_expr s_cdr(s_expr s);
bool s_atom(s_expr e);
bool s_eq(s_expr a, s_expr b);

void s_ref(s_expr s);
void s_free(s_expr s);

void s_ref_ref(s_expr_ref* r);
void s_ref_free(s_expr_type t, s_expr_ref* r);

void s_dump(s_expr s);
