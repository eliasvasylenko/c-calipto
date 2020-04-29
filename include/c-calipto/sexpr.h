typedef enum sexpr_type {
	CONS,
	SYMBOL,
	STRING,
	CHARACTER,
	INTEGER,
	NIL,
	BUILTIN,
	LAMBDA,
	FUNCTION
} sexpr_type;

typedef struct sexpr {
	const sexpr_type type;
	_Atomic(int32_t) ref_count;
} sexpr;

typedef struct cons {
	const sexpr const* car;
	const sexpr const* cdr;
} cons;

/*
 * An expression can be promoted to a lambda when it
 * appears as a term in a statement. This promotion
 * does not modify the behaviour of the expression,
 * but enumerates any free variables mentioned so that
 * later evaluation to a function can be optimised
 */
typedef struct lambda {
	sexpr* free_vars;
	sexpr* params;
	sexpr* body;
} function;

typedef struct function {
	bindings* capture;
	sexpr* params;
	sexpr* body;
} function;

typedef struct builtin {
	sexpr* symbol;
	sexpr* (*apply)(int32_t arg_count, term* args);
} builtin;

sexpr *sexpr_nil();

sexpr* sexpr_register_builtin(UChar* ns, UChar* n, sexpr* (*f)(int32_t arg_count, term* args));
sexpr* sexpr_lambda(sexpr* e);
sexpr* sexpr_function(lambda l, sexpr** args);

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
bool sexpr_atom(const sexpr* e);
bool sexpr_eq(const sexpr* a, const sexpr* b);

void sexpr_ref(const sexpr* s);
void sexpr_free(sexpr* s);
void sexpr_dump(const sexpr* s);
