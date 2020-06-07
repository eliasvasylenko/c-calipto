#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/ucnv.h>

#include "c-calipto/stringref.h"
#include "c-calipto/idtrie.h"
#include "c-calipto/sexpr.h"

static s_table table;
static s_expr data_nil;
static s_expr data_quote;
static s_expr data_lambda;

s_expr_ref* ref(int32_t payload_size) {
	s_expr_ref* r = malloc(sizeof(_Atomic(uint32_t)) + payload_size);
	r->ref_count = ATOMIC_VAR_INIT(1);
	return r;
}

s_expr s_function(s_function_type* t, uint32_t data_size, void* data) {
	s_expr_ref* r = ref(sizeof(s_function_data) + data_size);
	r->function.type = t;
	memcpy(&r->function + 1, data, data_size);

	return (s_expr){ FUNCTION, .p=r };
}

typedef struct s_key {
	s_expr_ref* qualifier;
	UChar name[1];
} s_key;

void* s_get_value(void* key, idtrie_node* owner) {
	s_expr_ref* r = ref(sizeof(idtrie_node*));
	r->symbol = owner;
	return r;
}

void s_update_value(void* value, idtrie_node* owner) {
	s_expr_ref* r = value;
	r->symbol = owner;
}

s_expr_ref* s_intern(s_expr_ref* qualifier, strref name) {
	int32_t maxlen = strref_maxlen(name);
	s_key* key = malloc(sizeof(s_key) + sizeof(UChar) * (maxlen - 1));
	int32_t len = strref_cpy(maxlen, key->name, name);
	if (maxlen != len) {
		s_key* key2 = malloc(sizeof(s_key) + sizeof(UChar) * (len - 1));
		memcpy(key2->name, key->name, len);
		free(key);
		key = key2;
	}

	key->qualifier = qualifier;

	return idtrie_insert(
			table.trie,
			sizeof(s_key) + sizeof(UChar) * (len - 1),
			key);
}

s_expr s_symbol(s_expr_ref* q, strref n) {
	return (s_expr){ SYMBOL, .p=s_intern(q, n) };
}

s_symbol_info* s_inspect(const s_expr e) {
	if (e.type != SYMBOL) {
		return NULL;
	}
	return idtrie_fetch_key(e.p->symbol);
}

s_expr s_character(UChar32 cp) {
	return (s_expr){ CHARACTER, .character=cp };
}

s_expr s_string(strref s) {
	int32_t maxlen = strref_maxlen(s);
	s_expr_ref* r = ref(sizeof(s_string_data) + sizeof(UChar) * maxlen);
	int32_t len = strref_cpy(maxlen, r->string.string, s);
	if (maxlen != len) {
		s_expr_ref* r2 = ref(sizeof(s_string_data) + sizeof(UChar) * len);
		memcpy(r2->string.string, r->string.string, len);
		free(r);
		r = r2;
	}
	r->string.string[len] = u'\0';
	return (s_expr){ STRING, .p=r };
}

s_expr s_cons(const s_expr car, const s_expr cdr) {
	if (car.type == CHARACTER && cdr.type == STRING) {
		bool single = !U_IS_SURROGATE(car.character);
		int32_t head = single ? 1 : 2;
		int32_t len = u_strlen(cdr.p->string.string);

		s_expr_ref* r = ref(sizeof(UChar) * (len + head));
		memcpy(r->string.string + head, cdr.p->string.string, sizeof(UChar) * len);
		if (single) {
			r->string.string[0] = car.character;
		} else {
			r->string.string[0] = U16_LEAD(car.character);
			r->string.string[1] = U16_TRAIL(car.character);
		}
		return (s_expr){ STRING, .p=r };
	}

	s_expr_ref* r = ref(sizeof(s_cons_data));
	r->cons.car = car;
	r->cons.cdr = cdr;
	s_alias(r->cons.car);
	s_alias(r->cons.cdr);
	return (s_expr){ CONS, .p=r };
}

s_expr s_car(const s_expr e) {
	switch (e.type) {
		case CONS:
			s_alias(e.p->cons.car);
			return e.p->cons.car;

		case STRING:
			;
			bool single = U16_IS_SINGLE(e.p->string.string[0]);
			UChar32 cp = single
				? e.p->string.string[0]
				: U16_GET_SUPPLEMENTARY(e.p->string.string[0], e.p->string.string[1]);
			return s_character(cp);

		default:
			printf("Cannot destruct atom ");
			s_dump(e);
			assert(false);
	}
}

s_expr s_cdr(const s_expr e) {
	switch (e.type) {
		case CONS:
			return s_alias(e.p->cons.cdr);

		case STRING:
			;
			bool single = U16_IS_SINGLE(e.p->string.string[0]);
			int32_t head = single ? 1 : 2;
			int32_t len = u_strlen(e.p->string.string);
			len -= head;
			if (len == 0) {
				return s_alias(data_nil);
			}
			s_expr_ref* r = ref(sizeof(s_string_data) + sizeof(UChar) * len);
			u_strncpy(r->string.string, e.p->string.string, len);
			r->string.string[len] = u'\0';

			return (s_expr){ STRING, .p=r };

		default:
			printf("Cannot destruct atom ");
			s_dump(e);
			assert(false);
	}
}

bool s_atom(const s_expr e) {
	return e.type == SYMBOL;
}

bool s_eq(const s_expr a, const s_expr b) {
	if (a.type != b.type) {
		return false;
	}
	switch (a.type) {
		case ERROR:
			return true;
		case SYMBOL:
			return a.p == b.p;
		case CONS:
			return s_eq(a.p->cons.car, b.p->cons.car) && s_eq(a.p->cons.cdr, b.p->cons.cdr);
		case FUNCTION:
			return a.p->function.type == b.p->function.type
				&& s_eq(a.p->function.type->represent(&a.p->function + 1),
					b.p->function.type->represent(&b.p->function + 1));
		case CHARACTER:
			return a.character == b.character;
		case STRING:
			return !u_strcmp(a.p->string.string, b.p->string.string);
		case INTEGER:
			return a.integer == b.integer;
	}
	return true;
}

int32_t s_delist_recur(int32_t index, s_expr s, s_expr** elems) {
	if (s_eq(s, data_nil)) {
		*elems = index == 0 ? NULL : malloc(sizeof(s_expr) * index);
		return index;
	}

	if (s_atom(s)) {
		return -1;
	}

	s_expr tail = s_cdr(s);
	int32_t size = s_delist_recur(index + 1, tail, elems);
	s_dealias(tail);

	if (size >= 0) {
		(*elems)[index] = s_car(s);
	}

	return size;
}

int32_t s_delist(s_expr s, s_expr** elems) {
	return s_delist_recur(0, s, elems);
}

void s_elem_dump(const s_expr s);

void s_tail_dump(const s_expr s) {
	if (s_eq(s, data_nil)) {
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
	s_dealias(car);
	s_dealias(cdr);
}

void s_elem_dump(const s_expr s) {
	UChar *string_payload;

	switch (s.type) {
	case STRING:;
		u_printf_u(u"\"%S\"", s.p->string.string);
		break;
	case CHARACTER:
		u_printf_u(u"unicode:%04x", s.character);
		break;
	case INTEGER:
		printf("%li", s.integer);
		break;
	default:
		if (s_atom(s)) {
			if (s_eq(s, data_nil)) {
				printf("()");
			} else {
				s_symbol_info* i = s_inspect(s);
				s_elem_dump(i->qualifier);
				u_printf_u(u":%S", i->name);
				free(i);
			}
		} else {
			s_expr car = s_car(s);
			s_expr cdr = s_cdr(s);
			printf("(");
			s_elem_dump(car);
			s_tail_dump(cdr);
			s_dealias(car);
			s_dealias(cdr);
		}
		break;
	}
}

void s_dump(const s_expr s) {
	s_elem_dump(s);
	printf("\n");
}

s_expr s_alias(s_expr e) {
	switch (e.type) {
		case CHARACTER:
		case INTEGER:
			break;
		default:
			s_ref(e.p);
	}
	return e;
}

void s_dealias(s_expr e) {
	switch (e.type) {
		case CHARACTER:
		case INTEGER:
			break;
		default:
			s_free(e.type, e.p);
	}
}

s_expr_ref* s_ref(s_expr_ref* r) {
	atomic_fetch_add(&r->ref_count, 1);
	return r;
}

void s_free(s_expr_type t, s_expr_ref* r) {
	if (atomic_fetch_add(&r->ref_count, -1) > 1) {
		return;
	}
	switch (t) {
		case CHARACTER:
		case INTEGER:
			return;
		case ERROR:
			break;
		case SYMBOL:
			idtrie_remove(r->symbol);
			break;
		case CONS:
			s_dealias(r->cons.car);
			s_dealias(r->cons.cdr);
			break;
		case FUNCTION:
			r->function.type->free(&r->function + 1);
			break;
		case STRING:
			break;
		case BIG_INTEGER:
			break;
	}
	free(r);
}

void s_init() {
	table = (s_table){ { NULL, s_get_value, s_update_value } };
	s_expr data = s_symbol(NULL, u_strref(u"data"));
	data_nil = s_symbol(data.p, u_strref(u"nil"));
	data_quote = s_symbol(data.p, u_strref(u"quote"));
	data_lambda = s_symbol(data.p, u_strref(u"lambda"));
	s_dealias(data);
}

void s_close() {
	idtrie_clear(table.trie);
}

