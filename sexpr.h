typedef enum sexpr_type {
	CONS,
	SYMBOL,
	STRING,
	CHAR,
	INT
} sexpr_type;

typedef struct sexpr {
	sexpr_type type;
	_Atomic(int32_t) ref_count;
} sexpr;

typedef struct cons {
	sexpr* car;
	sexpr* cdr;
} cons;

sexpr *sexpr_symbol(char32_t* nspace, char32_t* name);
sexpr *sexpr_empty_symbol(int32_t nslen, int32_t nlen);
sexpr *sexpr_cons(sexpr* car, sexpr* cdr);
sexpr *sexpr_car(sexpr* s);
sexpr *sexpr_cdr(sexpr* s);
void sexpr_free(sexpr* s);
void sexpr_dump(sexpr* s);
