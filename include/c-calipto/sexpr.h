typedef enum s_expr_type {
	NIL,
	CONS,
	SYMBOL,
	CHARACTER,
	STRING,
	INTEGER,
	BUILTIN,

	/*
	 * An expression can be promoted to a lambda when it
	 * appears as a term in a statement. This promotion
	 * does not modify the data, it is merely a
	 * specialization for the purpose of performance.
	 */
	LAMBDA,
	FUNCTION,

	QUOTE,

	ERROR
} s_expr_type;

typedef struct s_expr {
	s_expr_type type;
	_Atomic(int32_t)* ref_count;
	void* payload;
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
	_Atomic(int32_t) ref_count;
	struct s_bindings* parent;
	int32_t count;
} s_bindings;

typedef struct s_bound_expr {
	s_expr form;
	s_bindings* bindings;
} s_bound_expr;

typedef struct s_cons {
	const s_expr car;
	const s_expr cdr;
} s_cons;

typedef struct s_character {
	UChar32 codepoint;
} s_character;

typedef struct s_string {
	int32_t size;
	UChar* chars;
} s_string;

typedef struct s_integer {
	int64_t value;
} s_integer;

typedef struct s_symbol {
	s_string namespace;
	s_string name;
} s_symbol;

typedef struct s_lambda {
	int32_t free_var_count;
	s_expr* free_vars;
	int32_t param_count;
	s_expr* params;
	s_expr body;
} s_lambda;

/*
 * TODO This has linear lookup time for captures, this should
 * be rewritten with a proper table lookup
 */
typedef struct s_function {
	s_expr* capture;
	s_lambda lambda;
} s_function;

typedef struct s_builtin {
	s_string name;
	int32_t arg_count;
	s_bound_expr (*apply)(s_expr* args);
} s_builtin;

s_bindings* s_bindings_alloc(const s_bindings* parent, int32_t count, const s_binding* b);
void s_bindings_free(s_bindings* p);
s_expr s_resolve(s_expr name, s_bindings* b);
s_bound_expr s_bind(s_expr value, s_bindings* b);

s_expr s_alloc_nil();
s_expr s_alloc_symbol(s_symbol s);
s_expr s_alloc_cons(s_cons c);
s_expr s_alloc_character(s_character c);
s_expr s_alloc_string(s_string s);
s_expr s_alloc_builtin(s_builtin b);
s_expr s_alloc_lambda(s_lambda l);
s_expr s_alloc_function(s_function f);

s_expr s_alloc_error(char* c);

s_string s_strcpy(const UConverter* c, const char* s);
s_string s_strncpy(const UConverter* c, int32_t l, const char* s);
s_string s_u_strcpy(const UChar* s);
s_string s_u_strncpy(int32_t l, const UChar* s);

UChar* s_name(s_expr s);
UChar* s_namespace(s_expr s);

s_expr s_car(s_expr s);
s_expr s_cdr(s_expr s);
bool s_atom(s_expr e);
bool s_eq(s_expr a, s_expr b);

void s_ref(s_expr s);
void s_free(s_expr s);
void s_dump(s_expr s);
