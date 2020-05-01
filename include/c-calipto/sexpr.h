struct s_symbol_data;
struct s_cons_data;
struct s_builtin_data;
struct s_function_data;
struct s_lambda_data;

typedef enum s_expr_type {
	ERROR,
	SYMBOL,
	CONS,
	NIL,
	BUILTIN,
	FUNCTION,
	QUOTE,
	LAMBDA,
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
		struct s_builtin_data* builtin;
		struct s_function_data* function;
		struct s_expr* quote;
		struct s_lambda_data* lambda;
		UChar32 character;
		UChar* string;
		int64_t integer;
	};
} s_expr;

typedef struct s_binding {
	s_expr name;
	s_expr value;
} s_binding;

/*
 * TODO This has linear lookup time for bindings, this should
 * be rewritten with a proper table lookup
 */
typedef struct s_bindings {
	_Atomic(int32_t)* ref_count;
	struct s_bindings* parent;
	int32_t count;
	s_binding* bindings;
} s_bindings;

typedef struct s_bound_expr {
	s_expr form;
	s_bindings bindings;
} s_bound_expr;

typedef struct s_symbol_data {
	UChar* namespace;
	UChar* name;
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
	int32_t free_var_count;
	s_expr* free_vars;
	int32_t param_count;
	s_expr* params;
	s_expr body;
} s_lambda_data;

/*
 * TODO This has linear lookup time for captures, this should
 * be rewritten with a proper table lookup
 */
typedef struct s_function_data {
	s_bindings capture;
	s_expr lambda;
} s_function_data;

typedef struct s_builtin_data {
	UChar* name;
	int32_t arg_count;
	bool (*apply)(s_expr* args, s_bound_expr* result);
} s_builtin_data;

s_bindings s_alloc_bindings(const s_bindings* parent, int32_t count, const s_binding* b);
void s_ref_bindings(s_bindings p);
void s_free_bindings(s_bindings p);
s_expr s_resolve(const s_expr name, const s_bindings b);

s_expr s_nil();
s_expr s_symbol(strref ns, strref n);
s_expr s_cons(s_expr car, s_expr cdr);
s_expr s_character(UChar32 c);
s_expr s_string(strref s);
s_expr s_builtin(strref n, int32_t c, bool (*f)(s_expr* a, s_bound_expr* result));
s_expr s_lambda(
	int32_t free_var_count, s_expr* free_vars,
	int32_t param_count, s_expr* params,
	s_expr body);
s_expr s_function(s_bindings capture, s_expr lambda);
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
