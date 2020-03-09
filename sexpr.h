typedef enum sexpr_type {
        CONS,
        SYMBOL
} sexpr_type;

typedef struct cons cons;
typedef void symbol;
typedef struct sexpr sexpr;

struct sexpr {
        int ref_count;
        sexpr_type type;
        union {
                cons *cons_value;
                symbol *symbol_value;
        } value;
};

struct cons {
        sexpr *car, *cdr;
};

sexpr *sexpr_cons(sexpr *car, sexpr *cdr);
sexpr *sexpr_car(sexpr *expr);
sexpr *sexpr_cdr(sexpr *expr);
void sexpr_free(sexpr *expr);
