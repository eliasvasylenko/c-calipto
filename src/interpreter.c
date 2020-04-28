#include <stdbool.h>
#include <stdlib.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-calipto/sexpr.h"
#include "c-calipto/interpreter.h"

const UChar* data_namespace = u"data";
const UChar* function_namespace = u"function";
const UChar* system_namespace = u"system";

const UChar* lambda_name = u"lambda";
const UChar* quote_name = u"lambda";

bindings* make_bindings(const bindings* p, int32_t c, const binding* b) {
	bindings* bs = malloc(sizeof(bindings) + sizeof(binding) * c);
	bs->parent = (bindings*)p;
	bs->count = c;
	binding* a = (binding*)(bs + 1);
	for (int i = 0; i < c; i++) {
		a[i] = b[i];
	}
	return bs;
}

void free_bindings(bindings* p) {

}

term resolve(const bindings* b, const sexpr* name) {
	if (b == NULL) {
		return (term) { ERROR, NULL };
	}

	binding* a = (binding*)(b + 1);

	for (int i = 0; i < b->count; i++) {
		binding b = a[i];
		if (sexpr_eq(name, b.name)) {
			return b.value;
		}
	}

	return resolve(b->parent, name);
}

term eval_expression(const sexpr* e, const bindings* b) {
	if (sexpr_atom(e)) {
		return resolve(b, e);
	}
	sexpr* head = sexpr_car(e);
	sexpr_free(head);
	sexpr* tail = sexpr_cdr(e);
	sexpr_free(tail);

	term t;
	if (u_strcmp(sexpr_namespace(head), data_namespace) != 0) {
		t = (term){ ERROR, NULL };

	} else if (u_strcmp(sexpr_name(head), lambda_name) == 0) {
		function* f = malloc(sizeof(function));
		f->capture = b;
		f->params = sexpr_car(tail);
		tail = sexpr_cdr(tail);
		free(tail);
		f->body = sexpr_car(tail);
		t = (term){ FUNCTION, f };

	} else if (u_strcmp(sexpr_name(head), quote_name) == 0) {
		sexpr* tail = sexpr_cdr(e);
		sexpr_free(tail);
		sexpr* data = sexpr_car(tail);

		t = (term){ DATA, data };
	}

	return t;
}

sexpr* eval_statement(const sexpr* e, const bindings* b) {
	if (sexpr_atom(e)) {
		return NULL;
	}
	sexpr* head = sexpr_car(e);
	sexpr_free(head);
	sexpr* tail = sexpr_cdr(e);
	sexpr_free(tail);

	term target = eval_expression(head, b);
	sexpr* result;
	switch (target.type) {
	case FUNCTION:
		result = NULL;
	case BUILTIN:
		result = NULL;
	default:
		result = NULL;
	}
	term_free(target);

	return result;
}

void eval(const sexpr* e, const bindings* b) {
	return eval_statement(e, b);
}

