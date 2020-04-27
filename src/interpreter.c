#include <stdbool.h>
#include <stdlib.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-calipto/sexpr.h"
#include "c-calipto/interpreter.h"

bindings* collect_bindings(bindings* p, int32_t c, binding* b) {
	bindings* bs = malloc(sizeof(bindings) + sizeof(binding) * c);
	bs->parent = p;
	bs->binding_count = c;
	binding* a = (binding*)(bs + 1);
	for (int i = 0; i < c; i++) {
		a[i] = b[i];
	}
	return bs;
}

sexpr* resolve(bindings* b, sexpr* name) {
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

sexpr* eval(sexpr* e, bindings* b) {
	return sexpr_usymbol(u"test", u"result");
}
