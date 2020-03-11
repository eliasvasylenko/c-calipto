typedef enum sexpr_type {
	CONS,
	SYMBOL
} sexpr_type;

typedef struct sexpr {
	sexpr_type type;
	_Atomic(int32_t) ref_count;
} sexpr;

typedef struct cons {
	sexpr *car, *cdr;
} cons;

sexpr *sexpr_symbol_len(char32_t *name, int32_t length);
sexpr *sexpr_symbol(char32_t *name);
sexpr *sexpr_cons(sexpr *car, sexpr *cdr);
sexpr *sexpr_car(sexpr *expr);
sexpr *sexpr_cdr(sexpr *expr);
void sexpr_free(sexpr *expr);
