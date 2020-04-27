#include <stdbool.h>
#include <stdlib.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-calipto/sexpr.h"
#include "c-calipto/interpreter.h"

bindings* collect_bindings(const bindings* p, int32_t c, const binding* b) {
	bindings* bs = malloc(sizeof(bindings) + sizeof(binding) * c);
	bs->parent = p;
	bs->binding_count = c;
	binding* a = (binding*)(bs + 1);
	for (int i = 0; i < c; i++) {
		a[i] = b[i];
	}
	return bs;
}

sexpr* resolve(const bindings* b, const sexpr* name) {
	if (b == NULL) {
		return NULL;
	}

	binding* a = (binding*)(b + 1);

	for (int i = 0; i < b->binding_count; i++) {
		binding b = a[i];
		if (sexpr_eq(name, b.name)) {
			return b.value;
		}
	}

	return resolve(b->parent, name);
}

sexpr* eval(const sexpr* e, const bindings* b) {
	return eval_statement(e, b);
}

int32_t eval_args(const sexpr* e, sexpr** args) {
	;
}

sexpr* eval_statement(const sexpr* e, const bindings* b) {
	if (sexpr_atom(tail)) {
		return NULL;
	}
	sexpr* head = sexpr_car(e);
	sexpr* target = eval_expression(head);
	sexpr_free(head);

	sexpr* tail = sexpr_cdr(e);
	sexpr* args;
	int32_t arg_count = eval_args(tail);
	sexpr_free(tail);

	int size = 0;
	sexpr_ref(e);
	while (!sexpr_atom(tail)) {
		sexpr* list = tail;
		head = sexpr_car(list);
		tail = sexpr_cdr(list);
		sexpr_free(head);
		sexpr_free(list);

		size++;
	}

	sexpr_free(target);
	for (i = 0; i < arg_count; i++) {
		sexpr_free(args[i]);
	}
	free(args);

	return sexpr_usymbol(u"test", u"result");
}

