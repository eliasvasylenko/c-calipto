#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <uchar.h>
#include <limits.h>
#include <string.h>

#include <sexpr.h>

#include <stdio.h>

sexpr *sexpr_init(sexpr_type type, int32_t payload_size) {
	sexpr *expr = malloc(sizeof(sexpr) + payload_size);
	expr->type=type;
	expr->ref_count = ATOMIC_VAR_INIT(1);

	return expr;
}

sexpr* sexpr_symbol(char32_t* nspace, char32_t* name) {
	int i = 0;
	while (nspace[i] != U'\0') i++;

	int j = 0;
	while (name[j] != U'\0') j++;

	sexpr* expr = sexpr_empty_symbol(i, j);
	char32_t *payload = (char32_t*)(expr + 1);

	memcpy(payload, nspace, i * sizeof(char32_t));
	memcpy(payload + i + 1, name, j * sizeof(char32_t));

	return expr;
}

sexpr *sexpr_empty_symbol(int32_t nslen, int32_t nlen) {
	printf("new symbol [%i, %i]\n", nslen, nlen);

	sexpr *expr = sexpr_init(SYMBOL, sizeof(char32_t) * (nslen + nlen + 2));
	char32_t *payload = (char32_t*)(expr + 1);

	payload[nslen] = U'\0';
	payload[nslen + 1 + nlen] = U'\0';

	return expr;
}

sexpr *sexpr_cons(sexpr *car, sexpr *cdr) {
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
			break;
		}
		free(expr);
	}
}

void sexpr_elem_dump(sexpr* s) {
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
	case SYMBOL:;
		char32_t *payload = (char32_t*)(s + 1);
		char* mbr = malloc(sizeof(char) * (MB_LEN_MAX + 1));
		while (*payload != U'\0') {
			int size = c32rtomb(mbr, *payload, NULL);
			*(mbr + size) = '\0';
			printf("%s", mbr);
			payload++;
		}
		printf(":");
		payload++;
		while (*payload != U'\0') {
			int size = c32rtomb(mbr, *payload, NULL);
			*(mbr + size) = '\0';
			printf("%s", mbr);
			payload++;
		}
		free(mbr);
		break;
	}
}

void sexpr_dump(sexpr* s) {
	sexpr_elem_dump(s);
	printf("\n");
}
