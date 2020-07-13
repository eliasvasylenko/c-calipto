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

ovs_expr_ref* ref(uint32_t payload_size, uint32_t refs) {
	size_t size = offsetof(ovs_expr_ref, symbol) + payload_size;
	ovs_expr_ref* r = malloc(size);
	memset(r, 0, size);
	r->ref_count = ATOMIC_VAR_INIT(refs);
	return r;
}

ovs_expr ovs_function(ovs_context* c, ovs_function_type* t, uint32_t data_size, void* data) {
	ovs_expr_ref* r = ref(sizeof(ovs_function_data) + data_size, 1);
	r->function.type = t;
	r->function.context = c;
	memcpy(&r->function + 1, data, data_size);

	return (ovs_expr){ OVS_FUNCTION, .p=r };
}

void ovs_update_value(void* value, bdtrie_node* owner) {
	ovs_expr_ref* r = value;
	if (r->symbol.node != NULL) {
		r->symbol.node = owner;
	}
}

void* ovs_get_value(uint32_t key_size, const void* key_data, const void* value_data, bdtrie_node* owner) {
	ovs_expr_ref* r;
	if (value_data == NULL) {
		r = ref(sizeof(ovs_symbol_data), 0);
		r->symbol.node = owner;
		r->symbol.table = malloc(sizeof(ovs_table));
		r->symbol.table->qualifier = r;
		r->symbol.table->trie = (bdtrie) { NULL, ovs_get_value, ovs_update_value, free };
	} else {
		r = (ovs_expr_ref*)value_data;
	}
	return r;
}

void ovs_free_value(void* d) {
	ovs_expr_ref* r = d;
	if (r->symbol.node != NULL) {
		free(d);
	}
}

ovs_expr_ref* ovs_intern(ovs_table* table, uint32_t len, UChar* name, const ovs_expr_ref* root_symbol) {
	if (table->qualifier != NULL) {
		ovs_ref(table->qualifier);
	}

	uint32_t keysize = sizeof(UChar) * len;
	ovs_expr_ref* r = bdtrie_find_or_insert(&table->trie, keysize, name, root_symbol).data;
	ovs_ref(r);

	return r;
}

ovs_expr ovs_symbol(ovs_table* t, uint32_t l, UChar* n) {
	return (ovs_expr){ OVS_SYMBOL, .p=ovs_intern(t, l, n, NULL) };
}

ovs_expr ovs_root_symbol(ovs_root_table t) {
	return (ovs_expr){ OVS_SYMBOL, .p=&ovs_root_symbols[t].data };
}

ovs_table* ovs_table_for(ovs_context* c, const ovs_expr_ref* r) {
	if (r->symbol.node == NULL) {
		return c->root_tables + r->symbol.offset;
	} else {
		return r->symbol.table;
	}
}

ovs_table* ovs_table_of(ovs_context* c, const ovs_expr e) {
	switch (e.type) {
		case OVS_SYMBOL:
			if (e.p->symbol.node == NULL) {
				return c->root_tables + ovs_root_symbols[e.p->symbol.offset].qualifier;
			} else {
				return ovs_table_for(c, ((ovs_table*)bdtrie_trie(e.p->symbol.node))->qualifier);
			}

		case OVS_CONS:
			return ovs_table_for(c, e.p->cons.table->qualifier);

		case OVS_FUNCTION:
			;
			const ovs_function_data* f = &e.p->function;
			ovs_expr r = f->type->represent(f->context, f->type->name, f + 1);
			ovs_table* t = ovs_table_of(c, r);
			ovs_dealias(r);
			return t;

		case OVS_CHARACTER:
			return c->root_tables + OVS_TEXT_CHARACTER;

		case OVS_STRING:
			return c->root_tables + OVS_TEXT_STRING;

		default:
			assert(false);
	}
}

bool ovs_is_atom(ovs_table* t, const ovs_expr e) {
	return e.type == OVS_SYMBOL
		|| (t->qualifier == NULL
				? !ovs_is_qualified(e)
				: t->qualifier != ovs_qualifier(e).p);
}

bool ovs_is_symbol(ovs_expr e) {
	return e.type == OVS_SYMBOL;
}

bool ovs_is_qualified(ovs_expr e) {
	switch (e.type) {
		case OVS_SYMBOL:
			if (e.p->symbol.node == NULL) {
				return ovs_root_symbols[e.p->symbol.offset].qualifier != OVS_UNQUALIFIED;
			} else {
				return ((ovs_table*)bdtrie_trie(e.p->symbol.node))->qualifier != NULL;
			}

		case OVS_CONS:
			return e.p->cons.table->qualifier != NULL;

		case OVS_FUNCTION:
			;
			const ovs_function_data* f = &e.p->function;
			ovs_expr r = f->type->represent(f->context, f->type->name, f + 1);
			bool q = ovs_is_qualified(r);
			ovs_dealias(r);
			return q;

		default:
			return true;
	}
}

ovs_expr ovs_qualifier(ovs_expr e) {
	switch (e.type) {
		case OVS_SYMBOL:
			if (e.p->symbol.node == NULL) {
				return ovs_root_symbol(ovs_root_symbols[e.p->symbol.offset].qualifier);
			} else {
				return (ovs_expr){ OVS_SYMBOL, .p=((ovs_table*)bdtrie_trie(e.p->symbol.node))->qualifier };
			}

		case OVS_CONS:
			return (ovs_expr){ OVS_SYMBOL, .p=e.p->cons.table->qualifier };

		case OVS_FUNCTION:
			;
			const ovs_function_data* f = &e.p->function;
			ovs_expr r = f->type->represent(f->context, f->type->name, f + 1);
			ovs_expr q = ovs_qualifier(r);
			ovs_dealias(r);
			return q;

		case OVS_CHARACTER:
			return ovs_root_symbol(OVS_TEXT_CHARACTER);

		case OVS_STRING:
			return ovs_root_symbol(OVS_TEXT_STRING);

		default:
			assert(false);
	}
}

UChar* ovs_name(const ovs_expr e) {
	if (e.type != OVS_SYMBOL) {
		assert(false);
	}
	if (e.p->symbol.node == NULL) {
		UChar* name = ovs_root_symbols[e.p->symbol.offset].name;
		size_t nameSize = u_strlen(name);
		UChar* s = malloc(sizeof(UChar) * (nameSize + 1));
		u_strcpy(s, name, nameSize + 1);
		return s;
	}
	uint32_t size = bdtrie_key_size(e.p->symbol.node);
	if (size <= 0) {
		assert(false);
	} else {
		UChar* s = malloc(size + sizeof(UChar));
		bdtrie_key(s, e.p->symbol.node);
		UChar* end = (UChar*)((uint8_t*)s + size);
		*end = u'\0';
		return s;
	}
}

ovs_expr ovs_character(UChar32 cp) {
	return (ovs_expr){ OVS_CHARACTER, .character=cp };
}

ovs_expr ovs_cstring(UConverter* c, char* s) {
	int32_t size = strlen(s);
	int32_t destSize = size * 2;

	ovs_expr_ref* r = ref(offsetof(ovs_string_data, string) + sizeof(UChar) * (destSize + 1), 1);

	UErrorCode error = 0;
	uint32_t len = ucnv_toUChars(c,
			r->string.string, destSize,
			s, size,
			&error);

	if (len != destSize) {
		ovs_expr_ref* oldR = r;

		destSize = len;
		r = ref(offsetof(ovs_string_data, string) + sizeof(UChar) * (len + 1), 1);

		memcpy(r->string.string, oldR->string.string, destSize);

		free(oldR);
	}

	r->string.string[len] = u'\0';
	return (ovs_expr){ OVS_STRING, .p=r };
}

ovs_expr ovs_string(uint32_t len, UChar* s) {
	ovs_expr_ref* r = ref(offsetof(ovs_string_data, string) + sizeof(UChar) * (len + 1), 1);
	memcpy(r->string.string, s, len);
	r->string.string[len] = u'\0';
	return (ovs_expr){ OVS_STRING, .p=r };
}

ovs_expr ovs_cons(ovs_table* t, const ovs_expr car, const ovs_expr cdr) {
	if (car.type == OVS_CHARACTER && cdr.type == OVS_STRING) {
		bool single = !U_IS_SURROGATE(car.character);
		int32_t head = single ? 1 : 2;
		int32_t len = u_strlen(cdr.p->string.string);

		ovs_expr_ref* r = ref(sizeof(UChar) * (len + head), 1);
		memcpy(r->string.string + head, cdr.p->string.string, sizeof(UChar) * len);
		if (single) {
			r->string.string[0] = car.character;
		} else {
			r->string.string[0] = U16_LEAD(car.character);
			r->string.string[1] = U16_TRAIL(car.character);
		}
		return (ovs_expr){ OVS_STRING, .p=r };
	}

	ovs_expr_ref* r = ref(sizeof(ovs_cons_data), 1);
	r->cons.table = t;
	r->cons.car = car;
	r->cons.cdr = cdr;
	ovs_alias(car);
	ovs_alias(cdr);
	return (ovs_expr){ OVS_CONS, .p=r };
}

ovs_expr ovs_car(const ovs_expr e) {
	switch (e.type) {
		case OVS_CONS:
			return ovs_alias(e.p->cons.car);

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
				return ovs_alias(ovs_root_symbol(OVS_DATA_NIL));
			}
			ovs_expr_ref* r = ref(offsetof(ovs_string_data, string) + sizeof(UChar) * len, 1);
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

bool ovs_is_eq(const ovs_expr a, const ovs_expr b) {
	if (a.type != b.type) {
		return false;
	}
	switch (a.type) {
		case OVS_SYMBOL:
			return a.p == b.p;
		case OVS_CONS:
			return ovs_is_eq(a.p->cons.car, b.p->cons.car) && ovs_is_eq(a.p->cons.cdr, b.p->cons.cdr);
		case OVS_FUNCTION:
			;
			const ovs_function_data* fa = &a.p->function;
			const ovs_function_data* fb = &b.p->function;
			return fa->type == fb->type
				&& ovs_is_eq(fa->type->represent(fa->context, fa->type->name, fa + 1),
					fb->type->represent(fb->context, fb->type->name, fb + 1));
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

void ovs_tail_dump(const ovs_expr_ref* q, const ovs_expr s) {
	if (ovs_is_eq(s, ovs_root_symbol(OVS_DATA_NIL))) {
		printf(")");
		return;
	}
	if (ovs_is_symbol(s)
			|| (q == NULL
				? !ovs_is_qualified(s)
				: q == ovs_qualifier(s).p)) {
		printf(" . ");
		ovs_elem_dump(s);
		printf(")");
		return;
	}
	printf(" ");
	ovs_expr car = ovs_car(s);
	ovs_expr cdr = ovs_cdr(s);
	ovs_elem_dump(car);
	ovs_tail_dump(q, cdr);
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
			;
			ovs_expr_ref* qualifier = NULL;
			if (ovs_is_qualified(s)) {
				ovs_expr q = ovs_qualifier(s);
				qualifier = (ovs_expr_ref*)q.p;

				ovs_elem_dump(q);
				u_printf_u(u"/");
				ovs_dealias(q);
			}
			if (ovs_is_symbol(s)) {
				if (ovs_is_eq(s, ovs_root_symbol(OVS_DATA_NIL))) {
					printf("()");

				} else {
					UChar* n = ovs_name(s);
					u_printf_u(u"%S", n);
					free(n);
				}
			} else {
				ovs_expr car = ovs_car(s);
				ovs_expr cdr = ovs_cdr(s);
				printf("(");
				ovs_elem_dump(car);
				ovs_tail_dump(qualifier, cdr);
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

const ovs_expr_ref* ovs_ref(const ovs_expr_ref* r) {
	atomic_fetch_add(&((ovs_expr_ref*)r)->ref_count, 1);
	return r;
}

void ovs_free(ovs_expr_type t, const ovs_expr_ref* r) {
	if (atomic_fetch_add(&((ovs_expr_ref*)r)->ref_count, -1) > 1) {
		return;
	}
	switch (t) {
		case OVS_CHARACTER:
		case OVS_INTEGER:
			break;
		case OVS_SYMBOL:
			if (r->symbol.node != NULL) {
				bdtrie_delete(r->symbol.node);
				bdtrie_clear(&r->symbol.table->trie);
			}
			break;
		case OVS_CONS:
			if (r->cons.table->qualifier != NULL) {
				ovs_free(OVS_SYMBOL, r->cons.table->qualifier);
			}
			ovs_dealias(r->cons.car);
			ovs_dealias(r->cons.cdr);
			free((void*)r);
			break;
		case OVS_FUNCTION:
			r->function.type->free(&r->function + 1);
			free((void*)r);
			break;
		case OVS_STRING:
			free((void*)r);
			break;
		case OVS_BIG_INTEGER:
			break;
	}
}

ovs_context ovs_init() {
	ovs_context c = {
		malloc(sizeof(ovs_table) * OVS_ROOT_TABLE_COUNT)
	};
	for (int i = 0; i < OVS_ROOT_TABLE_COUNT; i++) {
		c.root_tables[i].trie = (bdtrie){ NULL, ovs_get_value, ovs_update_value, ovs_free_value };

		if (i == OVS_UNQUALIFIED) {
			c.root_tables[i].qualifier = NULL;

		} else {
			ovs_root_symbol_data* symbol = &ovs_root_symbols[i];
			c.root_tables[i].qualifier = &symbol->data;

			ovs_intern(
					c.root_tables + symbol->qualifier,
					u_strlen(symbol->name),
					symbol->name,
					&symbol->data);
		}
	}

	return c;
}

void ovs_close(ovs_context* c) {
	for (int i = 0; i < OVS_ROOT_TABLE_COUNT; i++) {
		bdtrie_clear(&c->root_tables[i].trie);
	}
}

ovs_expr ovs_list(ovs_table* t, int32_t count, ovs_expr* e) {
	ovs_expr l = ovs_alias(ovs_root_symbol(OVS_DATA_NIL));
	for (int i = count - 1; i >= 0; i--) {
		ovs_expr prev = l;
		l = ovs_cons(t, e[i], l);
		ovs_dealias(prev);
	}
	return l;
}

ovs_expr ovs_list_of(ovs_table* t, int32_t count, void** e, ovs_expr (*map)(const void* elem)) {
	ovs_expr l = ovs_alias(ovs_root_symbol(OVS_DATA_NIL));
	for (int i = count - 1; i >= 0; i--) {
		ovs_expr prev = l;
		ovs_expr head = map(e[i]);
		l = ovs_cons(t, head, l);
		ovs_dealias(head);
		ovs_dealias(prev);
	}
	return l;
}

int32_t ovs_delist_recur(ovs_table* t, int32_t index, ovs_expr s, ovs_expr** elems) {
	if (ovs_is_eq(s, ovs_root_symbol(OVS_DATA_NIL))) {
		*elems = index == 0 ? NULL : malloc(sizeof(ovs_expr) * index);
		return index;
	}

	if (ovs_is_atom(t, s)) {
		return -1;
	}

	ovs_expr tail = ovs_cdr(s);
	int32_t size = ovs_delist_recur(t, index + 1, tail, elems);
	ovs_dealias(tail);

	if (size >= 0) {
		(*elems)[index] = ovs_car(s);
	}

	return size;
}

int32_t ovs_delist(ovs_table* t, ovs_expr s, ovs_expr** elems) {
	return ovs_delist_recur(t, 0, s, elems);
}

int32_t ovs_delist_of_recur(ovs_table* t, int32_t index, ovs_expr s, void*** elems, void* (*map)(ovs_expr elem)) {
	if (ovs_is_eq(s, ovs_root_symbol(OVS_DATA_NIL))) {
		*elems = index == 0 ? NULL : malloc(sizeof(void*) * index);
		return index;
	}

	if (ovs_is_atom(t, s)) {
		return -1;
	}

	ovs_expr tail = ovs_cdr(s);
	int32_t size = ovs_delist_of_recur(t, index + 1, tail, elems, map);
	ovs_dealias(tail);

	if (size >= 0) {
		ovs_expr e = ovs_car(s);
		(*elems)[index] = map(e);
		ovs_dealias(e);
	}

	return size;
}

int32_t ovs_delist_of(ovs_table* t, ovs_expr s, void*** elems, void* (*map)(ovs_expr elem)) {
	return ovs_delist_of_recur(t, 0, s, elems, map);
}

