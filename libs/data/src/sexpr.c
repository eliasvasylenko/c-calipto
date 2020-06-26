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

#include "c-ohvu/io/stringref.h"
#include "c-ohvu/data/bdtrie.h"
#include "c-ohvu/data/sexpr.h"

typedef ovio_strref strref;

static ovs_table table;
static ovs_expr data_nil;
static ovs_expr data_quote;
static ovs_expr data_lambda;

ovs_expr_ref* ref(uint32_t payload_size) {
	ovs_expr_ref* r = malloc(offsetof(ovs_expr_ref, symbol) + payload_size);
	r->ref_count = ATOMIC_VAR_INIT(1);
	return r;
}

ovs_expr ovs_function(ovs_function_type* t, uint32_t data_size, void* data) {
	ovs_expr_ref* r = ref(sizeof(ovs_function_data) + data_size);
	r->function.type = t;
	memcpy(&r->function + 1, data, data_size);

	return (ovs_expr){ OVS_FUNCTION, .p=r };
}

void* ovs_get_value(uint32_t key_size, void* key_data, bdtrie_node* owner) {
	ovs_expr_ref* r = ref(sizeof(bdtrie_node*));
	r->symbol = owner;
	return r;
}

void ovs_update_value(void* value, bdtrie_node* owner) {
	ovs_expr_ref* r = value;
	r->symbol = owner;
}

ovs_expr_ref* ovs_intern(ovs_expr_ref* qualifier, strref name) {
	int32_t maxlen = ovio_strref_maxlen(name);
	ovs_symbol_info* key = malloc(offsetof(ovs_symbol_info, name) + sizeof(UChar) * maxlen);
	int32_t len = ovio_strref_cpy(maxlen, key->name, name);
	if (maxlen != len) {
		ovs_symbol_info* key2 = malloc(offsetof(ovs_symbol_info, name) + sizeof(UChar) * len);
		memcpy(key2->name, key->name, len);
		free(key);
		key = key2;
	}

	key->qualifier = qualifier;
	if (qualifier != NULL) {
		ovs_ref(qualifier);
	}

	uint32_t keysize = offsetof(ovs_symbol_info, name) + sizeof(UChar) * len;
	ovs_expr_ref* r = bdtrie_insert(&table.trie, keysize, key).data;

	free(key);

	return r;
}

ovs_expr ovs_symbol(ovs_expr_ref* q, strref n) {
	return (ovs_expr){ OVS_SYMBOL, .p=ovs_intern(q, n) };
}

ovs_symbol_info* ovs_inspect(const ovs_expr e) {
	if (e.type != OVS_SYMBOL) {
		assert(false);
	}
	uint32_t size = bdtrie_key_size(e.p->symbol);
	if (size <= 0) {
		assert(false);
	} else {
		ovs_symbol_info* s = malloc(size + sizeof(UChar));
		bdtrie_key(s, e.p->symbol);
		UChar* end = (UChar*)((uint8_t*)s + size);
		*end = u'\0';
		return s;
	}
}

ovs_expr ovs_character(UChar32 cp) {
	return (ovs_expr){ OVS_CHARACTER, .character=cp };
}

ovs_expr ovs_string(strref s) {
	int32_t maxlen = ovio_strref_maxlen(s);
	ovs_expr_ref* r = ref(offsetof(ovs_string_data, string) + sizeof(UChar) * (maxlen + 1));
	int32_t len = ovio_strref_cpy(maxlen, r->string.string, s);
	if (maxlen != len) {
		ovs_expr_ref* r2 = ref(offsetof(ovs_string_data, string) + sizeof(UChar) * (len + 1));
		memcpy(r2->string.string, r->string.string, len);
		free(r);
		r = r2;
	}
	r->string.string[len] = u'\0';
	return (ovs_expr){ OVS_STRING, .p=r };
}

ovs_expr ovs_cons(const ovs_expr car, const ovs_expr cdr) {
	if (car.type == OVS_CHARACTER && cdr.type == OVS_STRING) {
		bool single = !U_IS_SURROGATE(car.character);
		int32_t head = single ? 1 : 2;
		int32_t len = u_strlen(cdr.p->string.string);

		ovs_expr_ref* r = ref(sizeof(UChar) * (len + head));
		memcpy(r->string.string + head, cdr.p->string.string, sizeof(UChar) * len);
		if (single) {
			r->string.string[0] = car.character;
		} else {
			r->string.string[0] = U16_LEAD(car.character);
			r->string.string[1] = U16_TRAIL(car.character);
		}
		return (ovs_expr){ OVS_STRING, .p=r };
	}

	ovs_expr_ref* r = ref(sizeof(ovs_cons_data));
	r->cons.car = car;
	r->cons.cdr = cdr;
	ovs_alias(r->cons.car);
	ovs_alias(r->cons.cdr);
	return (ovs_expr){ OVS_CONS, .p=r };
}

ovs_expr ovs_car(const ovs_expr e) {
	switch (e.type) {
		case OVS_CONS:
			ovs_alias(e.p->cons.car);
			return e.p->cons.car;

		case OVS_STRING:
			;
			bool single = U16_IS_SINGLE(e.p->string.string[0]);
			UChar32 cp = single
				? e.p->string.string[0]
				: U16_GET_SUPPLEMENTARY(e.p->string.string[0], e.p->string.string[1]);
			return ovs_character(cp);

		case OVS_FUNCTION:
			printf("Cannot destruct function yet");
			assert(false);

		case OVS_CHARACTER:
			printf("Cannot destruct character yet");
			assert(false);

		default:
			printf("Cannot destruct atom %i ", e.type);
			assert(false);
	}
}

ovs_expr ovs_cdr(const ovs_expr e) {
	switch (e.type) {
		case OVS_CONS:
			return ovs_alias(e.p->cons.cdr);

		case OVS_STRING:
			;
			bool single = U16_IS_SINGLE(e.p->string.string[0]);
			int32_t head = single ? 1 : 2;
			int32_t len = u_strlen(e.p->string.string);
			len -= head;
			if (len == 0) {
				return ovs_alias(data_nil);
			}
			ovs_expr_ref* r = ref(offsetof(ovs_string_data, string) + sizeof(UChar) * len);
			u_strncpy(r->string.string, e.p->string.string, len);
			r->string.string[len] = u'\0';

			return (ovs_expr){ OVS_STRING, .p=r };

		case OVS_FUNCTION:
			printf("Cannot destruct function yet");
			assert(false);

		case OVS_CHARACTER:
			printf("Cannot destruct character yet");
			assert(false);
		
		default:
			printf("Cannot destruct atom %i ", e.type);
			assert(false);
	}
}

bool ovs_atom(const ovs_expr e) {
	return e.type == OVS_SYMBOL;
}

bool ovs_eq(const ovs_expr a, const ovs_expr b) {
	if (a.type != b.type) {
		return false;
	}
	switch (a.type) {
		case OVS_SYMBOL:
			return a.p == b.p;
		case OVS_CONS:
			return ovs_eq(a.p->cons.car, b.p->cons.car) && ovs_eq(a.p->cons.cdr, b.p->cons.cdr);
		case OVS_FUNCTION:
			return a.p->function.type == b.p->function.type
				&& ovs_eq(a.p->function.type->represent(&a.p->function + 1),
					b.p->function.type->represent(&b.p->function + 1));
		case OVS_CHARACTER:
			return a.character == b.character;
		case OVS_STRING:
			return !u_strcmp(a.p->string.string, b.p->string.string);
		case OVS_INTEGER:
			return a.integer == b.integer;
	}
	return false;
}

void ovs_elem_dump(const ovs_expr s);

void ovs_tail_dump(const ovs_expr s) {
	if (ovs_eq(s, data_nil)) {
		printf(")");
		return;
	}
	if (ovs_atom(s)) {
		printf(" . ");
		ovs_elem_dump(s);
		printf(")");
		return;
	}
	printf(" ");
	ovs_expr car = ovs_car(s);
	ovs_expr cdr = ovs_cdr(s);
	ovs_elem_dump(car);
	ovs_tail_dump(cdr);
	ovs_dealias(car);
	ovs_dealias(cdr);
}

void ovs_elem_dump(const ovs_expr s) {
	UChar *string_payload;

	switch (s.type) {
		case OVS_STRING:
			;
			u_printf_u(u"\"%S\"", s.p->string.string);
			break;
		case OVS_CHARACTER:
			u_printf_u(u"unicode:%04x", s.character);
			break;
		case OVS_INTEGER:
			printf("%li", s.integer);
			break;
		default:
			if (ovs_atom(s)) {
				if (ovs_eq(s, data_nil)) {
					printf("()");
				} else {
					ovs_symbol_info* i = ovs_inspect(s);
					if (i->qualifier == NULL) {
						u_printf_u(u"%S", i->name);
					} else {
						ovs_expr q = { OVS_SYMBOL, .p=i->qualifier };
						ovs_elem_dump(q);
						u_printf_u(u":%S", i->name);
					}
					free(i);
				}
			} else {
				ovs_expr car = ovs_car(s);
				ovs_expr cdr = ovs_cdr(s);
				printf("(");
				ovs_elem_dump(car);
				ovs_tail_dump(cdr);
				ovs_dealias(car);
				ovs_dealias(cdr);
			}
			break;
	}
}

void ovs_dump(const ovs_expr s) {
	ovs_elem_dump(s);
	printf("\n");
}

ovs_expr ovs_alias(ovs_expr e) {
	switch (e.type) {
		case OVS_CHARACTER:
		case OVS_INTEGER:
			break;
		default:
			ovs_ref(e.p);
	}
	return e;
}

void ovs_dealias(ovs_expr e) {
	switch (e.type) {
		case OVS_CHARACTER:
		case OVS_INTEGER:
			break;
		default:
			ovs_free(e.type, e.p);
	}
}

ovs_expr_ref* ovs_ref(ovs_expr_ref* r) {
	atomic_fetch_add(&r->ref_count, 1);
	return r;
}

void ovs_free(ovs_expr_type t, ovs_expr_ref* r) {
	if (atomic_fetch_add(&r->ref_count, -1) > 1) {
		return;
	}
	switch (t) {
		case OVS_CHARACTER:
		case OVS_INTEGER:
			return;
		case OVS_SYMBOL:
			bdtrie_delete(r->symbol);
			break;
		case OVS_CONS:
			ovs_dealias(r->cons.car);
			ovs_dealias(r->cons.cdr);
			break;
		case OVS_FUNCTION:
			r->function.type->free(&r->function + 1);
			break;
		case OVS_STRING:
			break;
		case OVS_BIG_INTEGER:
			break;
	}
	free(r);
}

void ovs_init() {
	table = (ovs_table){ { NULL, ovs_get_value, ovs_update_value, free } };
	ovs_expr data = ovs_symbol(NULL, ovio_u_strref(u"data"));
	data_nil = ovs_symbol(data.p, ovio_u_strref(u"nil"));
	data_quote = ovs_symbol(data.p, ovio_u_strref(u"quote"));
	data_lambda = ovs_symbol(data.p, ovio_u_strref(u"lambda"));
	ovs_dealias(data);
}

void ovs_close() {
	bdtrie_clear(table.trie);
}

ovs_expr ovs_list(int32_t count, ovs_expr* e) {
	ovs_expr l = ovs_alias(data_nil);
	for (int i = count - 1; i >= 0; i--) {
		ovs_expr prev = l;
		l = ovs_cons(e[i], l);
		ovs_dealias(prev);
	}
	return l;
}

ovs_expr ovs_list_of(int32_t count, void** e, ovs_expr (*map)(void* elem)) {
	ovs_expr l = ovs_alias(data_nil);
	for (int i = count - 1; i >= 0; i--) {
		ovs_expr prev = l;
		ovs_expr head = map(e[i]);
		l = ovs_cons(head, l);
		ovs_dealias(head);
		ovs_dealias(prev);
	}
	return l;
}

int32_t ovs_delist_recur(int32_t index, ovs_expr s, ovs_expr** elems) {
	if (ovs_eq(s, data_nil)) {
		*elems = index == 0 ? NULL : malloc(sizeof(ovs_expr) * index);
		return index;
	}

	if (ovs_atom(s)) {
		return -1;
	}

	ovs_expr tail = ovs_cdr(s);
	int32_t size = ovs_delist_recur(index + 1, tail, elems);
	ovs_dealias(tail);

	if (size >= 0) {
		(*elems)[index] = ovs_car(s);
	}

	return size;
}

int32_t ovs_delist(ovs_expr s, ovs_expr** elems) {
	return ovs_delist_recur(0, s, elems);
}

int32_t ovs_delist_of(ovs_expr l, void*** e, void* (*map)(ovs_expr elem)) {
	;
}

