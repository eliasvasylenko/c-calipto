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

#include "c-calipto/sexpr.h"

sexpr *sexpr_init(sexpr_type type, int32_t payload_size) {
	sexpr *expr = malloc(sizeof(sexpr) + payload_size);
	*(sexpr_type*)&expr->type = (const sexpr_type)type;
	*(_Atomic(int32_t)*)&expr->ref_count = ATOMIC_VAR_INIT(1);

	return expr;
}

sexpr* sexpr_nil() {
	return sexpr_init(NIL, 0);
}

const UChar* unicode_ns = u"unicode";
const int32_t unicode_nsl = 7;

sexpr* sexpr_regular_symbol(int32_t nsl, const UChar* ns, int32_t nl, const UChar* n) {
	sexpr *expr = sexpr_init(SYMBOL, sizeof(UChar) * (nsl + nl + 2));
	UChar *payload = (UChar*)(expr + 1);

	payload[nsl] = u'\0';
	payload[nsl + 1 + nl] = u'\0';

	u_strncpy(payload, ns, nsl);
	u_strncpy(payload + nsl + 1, n, nl);

	return expr;
}

sexpr* sexpr_unicode_codepoint_symbol(const UChar32 cp) {
	sexpr* e = sexpr_init(CHARACTER, sizeof(UChar));
	UChar *payload = (UChar*)(e + 1);

	*payload = cp;

	return e;
}

sexpr* sexpr_unicode_hex_symbol(const UChar* name) {
	UChar32 cp;
	if (u_sscanf(name, "%04x", &cp) == EOF) {
		int nl = u_strlen(name);
		return sexpr_regular_symbol(unicode_nsl, unicode_ns, nl, name);
	}
	return sexpr_unicode_codepoint_symbol(cp);
}

sexpr* sexpr_symbol(UConverter* c, const char* ns, const char* n) {
	return sexpr_nsymbol(c, strlen(ns), ns, strlen(n), n);
}

sexpr* sexpr_nsymbol(UConverter* c, int32_t nsl, const char* ns, int32_t nl, const char* n) {
	int unsl = nsl * 2;
	UChar* uns = malloc(sizeof(UChar) * unsl);
	int unl = nl * 2;
	UChar* un = malloc(sizeof(UChar) * unl);

	UErrorCode error = 0;
	unsl = ucnv_toUChars(c,
			uns, unsl,
			ns, nsl,
			&error);
	unl = ucnv_toUChars(c,
			un, unl,
			n, nl,
			&error);
	
	sexpr* e = sexpr_nusymbol(unsl, uns, unl, un);
	
	free(uns);
	free(un);

	return e;
}

sexpr* sexpr_usymbol(const UChar* ns, const UChar* n) {
	return sexpr_nusymbol(u_strlen(ns), ns, u_strlen(n), n);
}

sexpr* sexpr_nusymbol(int32_t nsl, const UChar* ns, int32_t nl, const UChar* n) {
	if (unicode_nsl == nsl && u_strcmp(unicode_ns, ns)) {
		return sexpr_unicode_hex_symbol(n);
	}
	return sexpr_regular_symbol(nsl, ns, nl, n);
}

sexpr* sexpr_string(UConverter* c, const char* s) {
	return sexpr_nstring(c, strlen(s), s);
}

sexpr* sexpr_nstring(UConverter* c, int32_t l, const char* s) {
	int ul = l * 2;
	UChar* us = malloc(sizeof(UChar) * ul);

	UErrorCode error = 0;
	ul = ucnv_toUChars(c,
			us, ul,
			s, l,
			&error);
	
	sexpr* e = sexpr_nustring(ul, us);
	free(us);

	return e;
}

sexpr* sexpr_ustring(const UChar* s) {
	return sexpr_nustring(u_strlen(s), s);
}

sexpr* sexpr_nustring(int32_t l, const UChar* s) {
	sexpr* e = sexpr_init(STRING, sizeof(UChar) * (l + 1));
	UChar* p = (UChar*)(e + 1);
	p[l] = u'\0';
	u_strncpy(p, s, l);

	return e;
}
sexpr *sexpr_cons(const sexpr *car, const sexpr *cdr) {
	if (car->type == CHARACTER && cdr->type == STRING) {
		// prepend car to string repr.
		return NULL;
	}

	sexpr *expr = sexpr_init(CONS, sizeof(cons));
	cons *payload = (cons*)(expr + 1);

	payload->car = car;
	payload->cdr = cdr;

	((sexpr*)car)->ref_count++;
	((sexpr*)cdr)->ref_count++;

	return expr;
}

sexpr *sexpr_car(const sexpr *expr) {
	cons *payload = (cons*)(expr + 1);

	sexpr *car = (sexpr*)payload->car;
	(*(_Atomic(int32_t)*)&car->ref_count)++;

	return car;
}

sexpr *sexpr_cdr(const sexpr *expr) {
	cons *payload = (cons*)(expr + 1);

	sexpr *cdr = (sexpr*)payload->cdr;
	(*(_Atomic(int32_t)*)&cdr->ref_count)++;

	return cdr;
}

bool sexpr_atom(const sexpr* e) {
	switch (e->type) {
	case SYMBOL:
	case CHARACTER:
	case NIL:
		return true;
	case CONS:;
	case STRING:
	case INTEGER:
		return false;
	}
}

bool sexpr_eq(const sexpr* a, const sexpr* b) {
	if (a->type != b->type) {
		return false;
	}
	switch (a->type) {
	case CONS:;
		cons *payload_a = (cons*)(a + 1);
		cons *payload_b = (cons*)(b + 1);
		return sexpr_eq(payload_a->car, payload_b->car);
	case SYMBOL:
	case STRING:
	case CHARACTER:
	case INTEGER:
		break;
	case NIL:
		return true;
	}
}

void sexpr_ref(const sexpr* e) {
	(*(_Atomic(int32_t)*)&e->ref_count)++;
}

void sexpr_free(sexpr *expr) {
	if (atomic_fetch_add(&expr->ref_count, -1) == 1) {
		switch (expr->type) {
		case CONS:;
			cons *payload = (cons*)(expr + 1);
			sexpr_free((sexpr*)payload->car);
			sexpr_free((sexpr*)payload->cdr);
			break;
		case SYMBOL:
		case STRING:
		case CHARACTER:
		case INTEGER:
		case NIL:
			break;
		}
		free(expr);
	}
}

void sexpr_elem_dump(const sexpr* s);

void sexpr_tail_dump(const sexpr* s) {
	switch (s->type) {
	case NIL:
		printf(")");
		break;
	case CONS:
		printf(" ");
		cons c = *(cons*)(s + 1);
		sexpr_elem_dump(c.car);
		sexpr_tail_dump(c.cdr);
		break;
	default:
		printf(" . ");
		sexpr_elem_dump(s);
		printf(")");
		break;
	}
}

void sexpr_elem_dump(const sexpr* s) {
	UChar *string_payload;

	switch (s->type) {
	case CONS:;
		sexpr* car = sexpr_car(s);
		sexpr* cdr = sexpr_cdr(s);
		printf("(");
		sexpr_elem_dump(car);
		sexpr_tail_dump(cdr);
		sexpr_free(car);
		sexpr_free(cdr);
		break;
	case SYMBOL:
		string_payload = (UChar*)(s + 1);
		string_payload += 1 + u_printf_u(u"%S", string_payload);
		u_printf_u(u":%S", string_payload);
		break;
	case STRING:;
		string_payload = (UChar*)(s + 1);
		u_printf_u(u"\"%S\"", string_payload);
		break;
	case CHARACTER:
		u_printf_u(u"unicode:%04x", *(UChar*)(s + 1));
		break;
	case INTEGER:
		printf("%li", *(long*)(s + 1));
		break;
	case NIL:
		printf("()");
		break;
	}
}

void sexpr_dump(const sexpr* s) {
	sexpr_elem_dump(s);
	printf("\n");
}
