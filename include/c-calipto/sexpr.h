typedef enum {
	CONS,
	SYMBOL,
	STRING,
	CHARACTER,
	INTEGER
} sexpr_type;

typedef struct {
	const sexpr_type type;
	_Atomic(int32_t) ref_count;
} sexpr;

typedef struct {
	const sexpr const* car;
	const sexpr const* cdr;
} cons;

sexpr *sexpr_symbol(const UChar* nspace, const UChar* name);
sexpr *sexpr_cons(const sexpr* car, const sexpr* cdr);
sexpr *sexpr_car(const sexpr* s);
sexpr *sexpr_cdr(const sexpr* s);
void sexpr_free(sexpr* s);
void sexpr_dump(const sexpr* s);
