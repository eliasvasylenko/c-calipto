#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/ucnv.h>

#include "c-calipto/stringref.h"
#include "c-calipto/sexpr.h"

_Atomic(int32_t)* counter() {
	_Atomic(int32_t)* c = malloc(sizeof(_Atomic(int32_t)));
	*c = ATOMIC_VAR_INIT(1);
	return c;
}

s_bindings s_alloc_bindings(const s_bindings* p, int32_t c, const s_binding* b) {
	if (p != NULL) {
		s_ref_bindings(*p);
	}
	int32_t memsize = sizeof(s_binding) * c;
	s_binding* bc = malloc(memsize);
	memcpy(bc, b, memsize);

	for (int i = 0; i < c; i++) {
		s_ref(bc[i].name);
		s_ref(bc[i].value);
	}

	return (s_bindings){ counter(), (s_bindings*)p, c, (s_binding*)bc };
}

void s_ref_bindings(const s_bindings b) {
	s_bindings mb = (s_bindings) b;
	atomic_fetch_add(mb.ref_count, 1);
}

void s_free_bindings(s_bindings p) {
	if (atomic_fetch_add(p.ref_count, -1) > 1) {
		return;
	}
	if (p.parent != NULL) {
		s_free_bindings(*p.parent);
		free(p.parent);
	}
	for (int i = 0; i < p.count; i++) {
		s_free(p.bindings[i].name);
		s_free(p.bindings[i].value);
	}
	free(p.ref_count);
	free(p.bindings);
}

s_expr s_resolve(const s_expr name, const s_bindings b) {
	for (int i = 0; i < b.count; i++) {
		s_binding x = b.bindings[i];
		if (s_eq(name, x.name)) {
			s_ref(x.value);
			return x.value;
		}
	}
	if (b.parent != NULL) {
		return s_resolve(name, *b.parent);
	}
	return s_error(u_strref(u"Failed to resolve binding"));
}

s_expr s_nil() {
	return (s_expr){ NIL, counter(), .nil=NULL };
}

s_expr s_error(strref message) {
	return (s_expr){ ERROR, counter(), .error=malloc_strrefcpy(message, NULL) };
}

s_expr s_function(s_bindings capture, s_expr lambda) {
	s_ref_bindings(capture);
	s_ref(lambda);
	s_function_data* fd = malloc(sizeof(s_function_data));
	fd->capture = capture;
	fd->lambda = lambda;
	return (s_expr){ FUNCTION, counter(), .function=fd };
}

s_expr s_builtin(strref n, int32_t c, bool (*f)(s_expr* a, s_bound_expr* result)) {
	s_builtin_data* bp = malloc(sizeof(s_builtin_data));
	bp->name = malloc_strrefcpy(n, NULL);
	bp->arg_count = c;
	bp->apply = f;
	return (s_expr){ BUILTIN, counter(), .builtin=bp };
}

const UChar const* unicode_ns = u"unicode";
const int32_t unicode_nsl = 7;

s_expr s_regular_symbol(strref ns, strref n) {
	s_symbol_data* sp = malloc(sizeof(s_symbol_data));
	sp->namespace = malloc_strrefcpy(ns, NULL);
	sp->name = malloc_strrefcpy(n, NULL);
	return (s_expr){ SYMBOL, counter(), .symbol=sp };
}

bool s_unicode_hex_codepoint(const UChar* name, UChar32* cp) {
	return u_sscanf(name, "%04x", cp) != EOF;
}

s_expr s_symbol(strref ns, strref n) {
	int32_t unsl;
	UChar* uns = malloc_strrefcpy(ns, &unsl);
	int32_t unl;
	UChar* un = malloc_strrefcpy(n, &unl);

	UChar32 codepoint;
	if (unicode_nsl == unsl
			&& u_strcmp(unicode_ns, uns)
			&& s_unicode_hex_codepoint(un, &codepoint)) {
		free(uns);
		free(un);
		return (s_expr){ CHARACTER, counter(), .character=codepoint };;
	}

	s_symbol_data* sp = malloc(sizeof(s_symbol_data));
	sp->namespace = uns;
	sp->name = un;
	return (s_expr){ SYMBOL, counter(), .symbol=sp };
}

s_expr s_character(UChar32 cp) {
	return (s_expr){ CHARACTER, counter(), .character=cp };
}

s_expr s_string(strref s) {
	return (s_expr){ STRING, counter(), .string=malloc_strrefcpy(s, NULL) };
}
s_expr s_cons(const s_expr car, const s_expr cdr) {
	if (car.type == CHARACTER && cdr.type == STRING) {
		bool single = !U_IS_SURROGATE(car.character);
		int32_t head = single ? 1 : 2;
		int32_t len = u_strlen(cdr.string);
		UChar* s = malloc(sizeof(UChar) * (len + head));
		u_strncpy(s + head, cdr.string, len);
		if (single) {
			*s = car.character;
		} else {
			*s = U16_LEAD(car.character);
			*(s + 1) = U16_TRAIL(car.character);
		}
		return (s_expr){ STRING, counter(), .string=s };
	}

	s_cons_data *cons = malloc(sizeof(s_cons_data));
	cons->car = car;
	cons->cdr = cdr;
	s_ref(cons->car);
	s_ref(cons->cdr);
	return (s_expr){ CONS, counter(), .cons=cons };
}

UChar* s_name(const s_expr e) {
	switch (e.type) {
		case ERROR:
			return e.error;

		case SYMBOL:
			return e.symbol->name;

		case NIL:
			return u"nil";

		case BUILTIN:
			return e.builtin->name;

		case FUNCTION:
			return u"_";

		case CHARACTER:
			return u"_char_";
		
		default:
			return NULL;
	}
}

UChar* s_namespace(const s_expr e) {
	switch (e.type) {
		case ERROR:
			return u"error";

		case SYMBOL:
			return e.symbol->namespace;

		case NIL:
			return u"data";

		case BUILTIN:
			return u"builtin";

		case FUNCTION:
			return u"function";

		case CHARACTER:
			return u"unicode";
		
		default:
			return NULL;
	}
}

s_expr s_car(const s_expr e) {
	switch (e.type) {
		case CONS:
			s_ref(e.cons->car);
			return e.cons->car;

		case QUOTE:
			return s_symbol(u_strref(u"data"), u_strref(u"quote"));

		case LAMBDA:
			return s_symbol(u_strref(u"data"), u_strref(u"lambda"));

		case STRING:
			;
			bool single = U16_IS_SINGLE(e.string[0]);
			UChar32 cp = single
				? e.string[0]
				: U16_GET_SUPPLEMENTARY(e.string[0], e.string[1]);
			return s_character(cp);

		default:
			return s_error(u_strref(u"Type error, cannot destructure atom"));
	}
}

s_expr s_cdr(const s_expr e) {
	switch (e.type) {
		case CONS:
			s_ref(e.cons->cdr);
			return e.cons->cdr;

		case QUOTE:
			s_ref(*e.quote);
			return *e.quote;

		case LAMBDA:
			;
			s_expr params = s_nil();
			for (int i = e.lambda->param_count; i > 0; --i) {
				s_expr old_params = params;
				params = s_cons(e.lambda->params[i], old_params);
				s_free(old_params);
			}
			return s_cons(params, s_cons(e.lambda->body, s_nil()));

		case STRING:
			;
			bool single = U16_IS_SINGLE(e.string[0]);
			int32_t head = single ? 1 : 2;
			int32_t len = u_strlen(e.string);
			if (len == 0) {
				return s_nil();
			}
			UChar* s = malloc(sizeof(UChar) * (len - head));
			u_strncpy(s, e.string, len - head);

			return (s_expr){ STRING, counter(), .string=s };

		default:
			return s_error(u_strref(u"Type error, cannot destructure atom"));
	}
}

bool s_atom(const s_expr e) {
	switch (e.type) {
		case ERROR:
		case SYMBOL:
		case NIL:
		case CHARACTER:
		case BUILTIN:
		case FUNCTION:
			return true;
		default:
			return false;
	}
}

bool s_eq(const s_expr a, const s_expr b) {
	if (a.type != b.type) {
		return false;
	}
	switch (a.type) {
		case CONS:
			return s_eq(a.cons->car, b.cons->car);
		case SYMBOL:
			return !u_strcmp(a.symbol->namespace, b.symbol->namespace)
				&& !u_strcmp(a.symbol->name, b.symbol->name);
		case STRING:
		case CHARACTER:
		case INTEGER:
			break;
		case NIL:
			return true;
	}
}

void s_ref(const s_expr e) {
	s_expr me = (s_expr) e;
	atomic_fetch_add(me.ref_count, 1);
}

void s_free(s_expr e) {
	if (atomic_fetch_add(e.ref_count, -1) > 1) {
		return;
	}
	switch (e.type) {
		case ERROR:
			free(e.error);
			break;
		case SYMBOL:
			free(e.symbol->name);
			free(e.symbol->namespace);
			free(e.symbol);
			break;
		case CONS:
			s_free(e.cons->car);
			s_free(e.cons->cdr);
			free(e.cons);
			break;
		case NIL:
			break;
		case BUILTIN:
			free(e.builtin->name);
			free(e.builtin);
			break;
		case FUNCTION:
			s_free_bindings(e.function->capture);
			s_free(e.function->lambda);
			free(e.function);
			break;
		case QUOTE:
			s_free(*e.quote);
			free(e.quote);
			break;
		case LAMBDA:
			free(e.lambda->free_vars);
			free(e.lambda->params);
			free(e.lambda);
			break;
		case CHARACTER:
			break;
		case STRING:
			free(e.string);
			break;
		case INTEGER:
			break;
		case BIG_INTEGER:
			break;
	}
	free(e.ref_count);
}

void s_elem_dump(const s_expr s);

void s_tail_dump(const s_expr s) {
	if (s.type == NIL) {
		printf(")");
		return;
	}
	if (s_atom(s)) {
		printf(" . ");
		s_elem_dump(s);
		printf(")");
		return;
	}
	printf(" ");
	s_expr car = s_car(s);
	s_expr cdr = s_cdr(s);
	s_elem_dump(car);
	s_tail_dump(cdr);
	s_free(car);
	s_free(cdr);
}

void s_elem_dump(const s_expr s) {
	UChar *string_payload;

	switch (s.type) {
	case STRING:;
		u_printf_u(u"\"%S\"", s.string);
		break;
	case CHARACTER:
		u_printf_u(u"unicode:%04x", s.character);
		break;
	case INTEGER:
		printf("%li", s.integer);
		break;
	case NIL:
		printf("()");
		break;
	default:
		if (s_atom(s)) {
			u_printf_u(u"%S:%S", s_namespace(s), s_name(s));
		} else {
			s_expr car = s_car(s);
			s_expr cdr = s_cdr(s);
			printf("(");
			s_elem_dump(car);
			s_tail_dump(cdr);
			s_free(car);
			s_free(cdr);
		}
		break;
	}
}

void s_dump(const s_expr s) {
	s_elem_dump(s);
	printf("\n");
}
