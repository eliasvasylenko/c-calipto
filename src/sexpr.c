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
	return malloc(sizeof(_Atomic(int32_t)));
}

s_bindings s_alloc_bindings(const s_bindings* p, int32_t c, const s_binding* b) {
	int32_t memsize = sizeof(s_binding) * c;
	s_binding* bc = malloc(memsize);
	memcpy(bc, b, memsize);
	return (s_bindings){ counter(), (s_bindings*)p, c, (s_binding*)b };
}

void s_free_bindings(s_bindings p) {
	if (atomic_fetch_add(p.ref_count, -1) > 1) {
		return;
	}
	if (p.parent != NULL) {
		s_free_bindings(*p.parent);
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
	return s_error("Failed to resolve binding");
}

s_expr s_resolve_expression(s_bound_expr e) {
	if (s_atom(e.form)) {
		return s_resolve(e.form, e.bindings);
	}

	switch (e.form.type) {
	case LAMBDA:
		;
		s_lambda_data l = *e.form.lambda;
		s_expr* capture = malloc(sizeof(s_expr) * l.free_var_count);
		for (int i = 0; i < l.free_var_count; i++) {
			capture[i] = s_resolve(l.free_vars[i], e.bindings);
		}
		s_function_data* f = malloc(sizeof(s_function_data));
		f->capture = capture;
		f->lambda = l;
		return (s_expr) { FUNCTION, counter(), .function=f };

	case QUOTE:
		;
		s_expr data = *e.form.quote;
		s_ref(data);
		return data;

	default:
		return s_error("Unable to resolve expression");
	}
}

s_expr s_nil() {
	return (s_expr){ NIL, counter(), .nil=NULL };
}

const UChar const* builtin_ns = u"primitive";
int32_t builtin_nsl = 9;

s_expr s_builtin(strref n, int32_t c, s_bound_expr (*f)(s_expr* a)) {
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

s_expr s_string(strref s) {
	return (s_expr){ STRING, counter(), .string=malloc_strrefcpy(s, NULL) };
}
s_expr s_cons(const s_expr car, const s_expr cdr) {
	if (car.type == CHARACTER && cdr.type == STRING) {
		bool single = U16_IS_SINGLE(car.character);
		int32_t head = single ? 1 : 2;
		int32_t len = u_strlen(cdr.string);
		UChar* s = malloc(sizeof(UChar) * (head + len));
		u_strcpy(s + head, cdr.string, len);
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
	*cons->car.ref_count++;
	*cons->cdr.ref_count++;
	return (s_expr){ CONS, counter(), .cons=cons };
}

s_expr s_car(const s_expr e) {
	if (s_atom(e)) {
		s_error("Type error, cannot destructure atom");
	}
	s_expr car = e.cons->car;
	return car;
}

s_expr s_cdr(const s_expr e) {
	if (s_atom(e)) {
		s_error("Type error, cannot destructure atom");
	}
	s_expr cdr = e.cons->cdr;
	return cdr;
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
		case CONS:;
			return s_eq(a.cons->car, b.cons->car);
		case SYMBOL:
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
	*me.ref_count++;
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
			free(e.symbol);
			break;
		case CONS:
			s_free(e.cons->car);
			s_free(e.cons->cdr);
			free(e.cons);
			break;
		case NIL:
		case BUILTIN:
			free(e.builtin);
		case FUNCTION:
			free(e.function);
		case QUOTE:
			s_free(*e.quote);
			free(e.quote);
		case LAMBDA:
			free(e.lambda);
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
	s_elem_dump(s_car(s));
	s_tail_dump(s_cdr(s));
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
			u_printf_u(u"%S:%S", s_name(s), s_namespace(s));
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
