#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>

#include "c-calipto/sexpr.h"

sexpr *sexpr_init(sexpr_type type, int32_t payload_size) {
	sexpr *expr = malloc(sizeof(sexpr) + payload_size);
	expr->type=type;
	expr->ref_count = ATOMIC_VAR_INIT(1);

	return expr;
}

const char16_t* unicode_nspace = u"unicode";

sexpr* sexpr_unicode_hex_symbol(const char16_t* name) {
	int32_t cp;
	if (sscanf(name, "%04x", &cp) == EOF) {
		return sexpr_regular_symbol(unicode_nspace, name);
	}
	return sexpr_unicode_codepoint_symbol(cp);
}

sexpr* sexpr_unicode_codepoint_symbol(const char16_t32 cp) {
	sexpr* s = sexpr_init(CHARACTER, sizeof(char16_t));
	char16_t *payload = (char16_t*)(s + 1);

	*payload = cp;

	return s;
}

sexpr* sexpr_symbol(const char16_t* nspace, const char16_t* name) {
	if (strcmp32(unicode_nspace, nspace)) {
		return sexpr_unicode_hex_symbol(name);
	}
	return sexpr_regular_symbol(nspace, name);
}

sexpr* sexpr_regular_symbol(char16_t* nspace, char16_t* name) {
	int i = strlen32(nspace);
	int j = strlen32(name);

	sexpr* expr = sexpr_empty_symbol(i, j);
	char16_t *payload = (char16_t*)(expr + 1);

	memcpy(payload, nspace, i * sizeof(char16_t));
	memcpy(payload + i + 1, name, j * sizeof(char16_t));

	return expr;
}

sexpr *sexpr_empty_symbol(int32_t nslen, int32_t nlen) {
	sexpr *expr = sexpr_init(SYMBOL, sizeof(char16_t) * (nslen + nlen + 2));
	char16_t *payload = (char16_t*)(expr + 1);

	payload[nslen] = U'\0';
	payload[nslen + 1 + nlen] = U'\0';

	return expr;
}

sexpr *sexpr_cons(sexpr *car, sexpr *cdr) {
	if (car->type == CHARACTER && cdr->type == STRING) {
		// prepend car to string repr.
		return NULL;
	}

	sexpr *expr = sexpr_init(CONS, sizeof(cons));
	cons *payload = (cons*)(expr + 1);

	payload->car = car;
	payload->cdr = cdr;

	car->ref_count++;
	cdr->ref_count++;

	return expr;
}

sexpr *sexpr_car(sexpr *expr) {
	cons *payload = (cons*)(expr + 1);

	sexpr *car = payload->car;
	car->ref_count++;

	return car;
}

sexpr *sexpr_cdr(sexpr *expr) {
	cons *payload = (cons*)(expr + 1);

	sexpr *cdr = payload->cdr;
	cdr->ref_count++;

	return cdr;
}

void sexpr_free(sexpr *expr) {
	if (atomic_fetch_add(&expr->ref_count, -1) == 1) {
		switch (expr->type) {
		case CONS:;
			cons *payload = (cons*)(expr + 1);
			sexpr_free(payload->car);
			sexpr_free(payload->cdr);
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
	char16_t *string_payload;
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
		string_payload = (char16_t*)(s + 1);
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
		string_payload = (char16_t*)(s + 1);
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
		printf("<char>");
		break;
	case INTEGER:
		printf("%li", (long)(s + 1));
	}
}

void sexpr_dump(sexpr* s) {
	sexpr_elem_dump(s);
	printf("\n");
}
