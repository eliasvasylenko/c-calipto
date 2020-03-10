#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <uchar.h>
#include <string.h>

#include <scanner.h>
#include <sexpr.h>

sexpr *sexpr_init(sexpr_type type, int32_t payload_size) {
        sexpr *expr = malloc(sizeof(sexpr) + payload_size);
        expr->type=type;
        expr->ref_count = ATOMIC_VAR_INIT(1);

        return expr;
};

void *sexpr_payload(sexpr *expr) {
        return expr + sizeof(sexpr);
};

sexpr *sexpr_symbol(char32_t *name) {
        int i = 0;
        while (name[i] != 0) {
                i++;
        }
        return sexpr_symbol_len(name, i);
};

sexpr *sexpr_symbol_len(char32_t *name, int32_t length) {
        sexpr *expr = sexpr_init(SYMBOL, sizeof(char32_t) * (length + 1));
        char32_t *payload = (char32_t*)sexpr_payload(expr);

        memcpy(payload, name, sizeof(char32_t) * length);
        payload[length] = 0;

        return expr;
};

sexpr *sexpr_cons(sexpr *car, sexpr *cdr) {
        sexpr *expr = sexpr_init(CONS, sizeof(cons));
        cons *payload = (cons*)sexpr_payload(expr);

        payload->car = car;
        payload->cdr = cdr;

        return expr;
};

sexpr *sexpr_car(sexpr *expr) {
        cons *payload = (cons*)sexpr_payload(expr);

        sexpr *car = payload->car;
        car->ref_count++;

        return car;
};

sexpr *sexpr_cdr(sexpr *expr) {
        cons *payload = (cons*)sexpr_payload(expr);

        sexpr *cdr = payload->cdr;
        cdr->ref_count++;

        return cdr;
};

void sexpr_free(sexpr *expr) {
        if (!atomic_fetch_add(&expr->ref_count, -1)) {
                switch (expr->type) {
                case CONS:;
                        cons *payload = (cons*)sexpr_payload(expr);
                        sexpr_free(payload->car);
                        sexpr_free(payload->cdr);
                        break;
                case SYMBOL:
                        break;
                }
                free(expr);
        }
};
