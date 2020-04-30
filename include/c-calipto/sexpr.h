typedef enum sexpr_type {
	CONS,
	SYMBOL,
	STRING,
	CHARACTER,
	INTEGER,
	NIL,
	BUILTIN,

	/*
	 * An expression can be promoted to a lambda when it
	 * appears as a term in a statement. This promotion
	 * does not modify the data, it is merely a
	 * specialization for the purpose of performance.
	 */
	LAMBDA,
	FUNCTION,

	QUOTE
} sexpr_type;

typedef struct sexpr {
	const sexpr_type type;
	_Atomic(int32_t) ref_count;
} sexpr;

typedef struct cons {
	const sexpr const* car;
	const sexpr const* cdr;
} cons;

typedef struct lambda {
	int32_t free_var_count;
	sexpr** free_vars;
	int32_t param_count;
	sexpr** params;
	sexpr* body;
} lambda;

/*
 * TODO This has linear lookup time for captures, this should
 * be rewritten with a proper table lookup
 */
typedef struct function {
	sexpr** capture;
	lambda lambda;
} function;

typedef struct builtin {
	UChar* name;
	int32_t arg_count;
	sexpr* (*apply)(sexpr** args);
} builtin;

sexpr *sexpr_nil();

sexpr* sexpr_builtin(UChar* n, int32_t arg_count, sexpr* (*f)(sexpr** args));
sexpr* sexpr_lambda(
		int32_t free_var_count, sexpr** free_vars,
		int32_t param_count, sexpr** params,
		sexpr* body);
sexpr* sexpr_function(lambda l, sexpr** capture);

sexpr *sexpr_symbol(UConverter* c, const char* ns, const char* n);
sexpr *sexpr_nsymbol(UConverter* c, int32_t nsl, const char* ns, int32_t nl, const char* n);
sexpr *sexpr_usymbol(const UChar* ns, const UChar* n);
sexpr *sexpr_nusymbol(int32_t nsl, const UChar* ns, int32_t nl, const UChar* n);

UChar* sexpr_name(sexpr* s);
UChar* sexpr_namespace(sexpr* s);

sexpr *sexpr_string(UConverter* c, const char* s);
sexpr *sexpr_nstring(UConverter* c, int32_t l, const char* s);
sexpr *sexpr_ustring(const UChar* s);
sexpr *sexpr_nustring(int32_t l, const UChar* s);

sexpr *sexpr_cons(const sexpr* car, const sexpr* cdr);
sexpr *sexpr_car(const sexpr* s);
sexpr *sexpr_cdr(const sexpr* s);
sexpr *sexpr_car_borrow(const sexpr* s);
sexpr *sexpr_cdr_borrow(const sexpr* s);
bool sexpr_atom(const sexpr* e);
bool sexpr_eq(const sexpr* a, const sexpr* b);

void sexpr_ref(const sexpr* s);
void sexpr_free(sexpr* s);
void sexpr_dump(const sexpr* s);
