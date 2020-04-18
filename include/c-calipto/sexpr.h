typedef enum {
	CONS,
	SYMBOL,
	STRING,
	CHARACTER,
	INTEGER,
	NIL
} sexpr_type;

typedef struct {
	const sexpr_type type;
	_Atomic(int32_t) ref_count;
} sexpr;

typedef struct {
	const sexpr const* car;
	const sexpr const* cdr;
} cons;

sexpr *sexpr_nil();

sexpr *sexpr_symbol(UConverter* c, const char* ns, const char* n);
sexpr *sexpr_nsymbol(UConverter* c, int32_t nsl, const char* ns, int32_t nl, const char* n);
sexpr *sexpr_usymbol(const UChar* ns, const UChar* n);
sexpr *sexpr_nusymbol(int32_t nsl, const UChar* ns, int32_t nl, const UChar* n);

sexpr *sexpr_string(UConverter* c, const char* s);
sexpr *sexpr_nstring(UConverter* c, int32_t l, const char* s);
sexpr *sexpr_ustring(const UChar* s);
sexpr *sexpr_nustring(int32_t l, const UChar* s);

sexpr *sexpr_cons(const sexpr* car, const sexpr* cdr);
sexpr *sexpr_car(const sexpr* s);
sexpr *sexpr_cdr(const sexpr* s);

void sexpr_free(sexpr* s);
void sexpr_dump(const sexpr* s);
