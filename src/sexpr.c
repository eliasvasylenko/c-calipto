#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>

#include "c-calipto/sexpr.h"

sexpr *sexpr_init(sexpr_type type, int32_t payload_size) {
	sexpr *expr = malloc(sizeof(sexpr) + payload_size);
	*(sexpr_type*)&expr->type = (const sexpr_type)type;
	*(_Atomic(int32_t)*)&expr->ref_count = ATOMIC_VAR_INIT(1);

	return expr;
}

const UChar* unicode_nspace = u"unicode";

sexpr* sexpr_regular_symbol(const UChar* nspace, const UChar* name) {
	int i = u_strlen(nspace);
	int j = u_strlen(name);

	sexpr *expr = sexpr_init(SYMBOL, sizeof(UChar) * (i + j + 2));
	UChar *payload = (UChar*)(expr + 1);

	payload[i] = U'\0';
	payload[i + 1 + j] = U'\0';

	u_strcpy(payload, nspace);
	u_strcpy(payload + i + 1, name);

	return expr;
}

sexpr* sexpr_unicode_codepoint_symbol(const UChar32 cp) {
	sexpr* s = sexpr_init(CHARACTER, sizeof(UChar));
	UChar *payload = (UChar*)(s + 1);

	*payload = cp;

	return s;
}

sexpr* sexpr_unicode_hex_symbol(const UChar* name) {
	UChar32 cp;
	if (u_sscanf(name, "%04x", &cp) == EOF) {
		return sexpr_regular_symbol(unicode_nspace, name);
	}
	return sexpr_unicode_codepoint_symbol(cp);
}

sexpr* sexpr_symbol(const UChar* nspace, const UChar* name) {
	if (u_strcmp(unicode_nspace, nspace)) {
		return sexpr_unicode_hex_symbol(name);
	}
	return sexpr_regular_symbol(nspace, name);
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
			break;
		}
		free(expr);
	}
}

void sexpr_elem_dump(sexpr* s) {
	UChar *string_payload;
	char* mbr;

	switch (s->type) {
	case CONS:;
		printf("(");
		sexpr* car = sexpr_car(s);
		sexpr_elem_dump(car);
		sexpr_free(car);
		printf(" . ");
		sexpr* cdr = sexpr_cdr(s);
		sexpr_elem_dump(cdr);
		sexpr_free(cdr);
		printf(")");
		break;
	case SYMBOL:
		string_payload = (UChar*)(s + 1);
		mbr = malloc(sizeof(char) * (MB_LEN_MAX + 1));
		while (*string_payload != U'\0') {
			int size = c32rtomb(mbr, *string_payload, NULL);
			*(mbr + size) = '\0';
			printf("%s", mbr);
			string_payload++;
		}
		printf(":");
		string_payload++;
		while (*string_payload != U'\0') {
			int size = c32rtomb(mbr, *string_payload, NULL);
			*(mbr + size) = '\0';
			printf("%s", mbr);
			string_payload++;
		}
		free(mbr);
		break;
	case STRING:;
		string_payload = (UChar*)(s + 1);
		mbr = malloc(sizeof(char) * (MB_LEN_MAX + 1));
		printf("\"");
		while (*string_payload != U'\0') {
			int size = c32rtomb(mbr, *string_payload, NULL);
			*(mbr + size) = '\0';
			printf("%s", mbr);
			string_payload++;
		}
		printf("\"");
		free(mbr);
		break;
	case CHARACTER:
		u_printf("%04x", *(UChar32*)(s + 1));
		break;
	case INTEGER:
		printf("%li", *(long*)(s + 1));
	}
}

void sexpr_dump(sexpr* s) {
	sexpr_elem_dump(s);
	printf("\n");
}
