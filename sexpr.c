#include <sexpr.h>
#include <stdlib.h>

sexpr *sexpr_cons(sexpr *car, sexpr *cdr) {
        cons *cons_value = malloc(sizeof(cons));
        cons_value->car = car;
        cons_value->cdr = cdr;

        sexpr *cons = malloc(sizeof(sexpr));
        cons->ref_count = 1;
        cons->type = CONS;
        cons->value.cons_value = cons_value;

        return cons;
}
