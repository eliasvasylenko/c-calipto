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

s_expr s_error() {
	return (s_expr){ ERROR, .p=NULL };
}

s_expr s_function(s_lambda_term* lambda, s_expr* capture) {
	s_expr_ref* r = ref(sizeof(s_function_data));
	r->function.lambda = s_ref_lambda(lambda);
	r->function.capture = capture;

	int32_t capture_count = lambda->var_count - lambda->param_count;
	for (int i = 0; i < capture_count; i++) {
		s_alias(capture[i]);
	}
	return (s_expr){ FUNCTION, .p=r };
}

s_expr s_builtin(s_expr_ref* n,
		s_expr (*rp)(void* d),
		int32_t c,
		bool (*a)(s_statement* result, s_expr* a, void* d),
		void (*f)(void* d),
		void* d) {
	s_expr_ref* r = ref(sizeof(s_builtin_data));
	r->builtin.name = n;
	r->builtin.represent = rp;
	r->builtin.arg_count = c;
	r->builtin.apply = a;
	r->builtin.free = f;
	r->builtin.data = d;
	return (s_expr){ BUILTIN, .p=r };
}

typedef struct s_key {
	s_expr_ref* qualifier;
	UChar name[1];
} s_key;

void* s_value(void* key, id id) {
	s_expr_ref* r = ref(sizeof(id));
	r->symbol = id;
	return r;
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

	id id = idtrie_insert(
			table.trie,
			sizeof(s_key) + sizeof(UChar) * (len - 1),
			key,
			s_value);

	return (s_expr_ref*)id.leaf->value;
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

s_term s_quote(s_expr e) {
	return (s_term){ .quote=s_alias(e) };
}

s_term s_lambda(uint32_t param_count, s_expr_ref** params,
		uint32_t var_count, uint32_t* vars,
		uint32_t term_count, s_term* terms) {
	s_lambda_term* l = malloc(sizeof(s_lambda_term));
	l->ref_count = ATOMIC_VAR_INIT(1);
	l->param_count = param_count;
	if (param_count > 0) {
		l->params = malloc(sizeof(s_expr_ref*) * param_count);
		for (int i = 0; i < param_count; i++) {
			s_ref(params[i]);
			l->params[i] = params[i];
		}
	} else {
		l->params = NULL;
	}
	l->var_count = var_count;
	if (var_count > 0) {
		l->vars = malloc(sizeof(uint32_t) * var_count);
		for (int i = 0; i < var_count; i++) {
			l->vars[i] = vars[i];
		}
	} else {
		l->vars = NULL;
	}
	l->term_count = term_count;
	if (term_count > 0) {
		l->terms = malloc(sizeof(s_term) * term_count);
		for (int i = 0; i < term_count; i++) {
			s_alias_term(terms[i]);
			l->terms[i] = terms[i];
		}
	} else {
		l->terms = NULL;
	}
	return (s_term){ .type=LAMBDA, .lambda=l };
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
			;
			int32_t* a = NULL;
			*a = 0;
			return s_error(u_strref(u"Type error, cannot destructure atom"));
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
			return s_error(u_strref(u"Type error, cannot destructure atom"));
	}
}

bool s_atom(const s_expr e) {
	return e.type == SYMBOL;
}

bool s_eq_lambda(const s_lambda_term* a, const s_lambda_term* b);

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
			;
			uint32_t var_count = a.p->function.lambda->var_count;
			if (var_count != b.p->function.lambda->var_count) {
				return false;
			}
			for (int i = 0; i < var_count; i++) {
				if (!s_eq(a.p->function.capture[i], b.p->function.capture[i])) {
					return false;
				}
			}
			return s_eq_lambda(a.p->function.lambda, b.p->function.lambda);
		case BUILTIN:
			return a.p == b.p;
		case CHARACTER:
			return a.character == b.character;
		case STRING:
			return !u_strcmp(a.p->string.string, b.p->string.string);
		case INTEGER:
			return a.integer == b.integer;
	}
	return true;
}

bool s_eq_lambda(const s_lambda_term* a, const s_lambda_term* b) {
	uint32_t param_count = a->param_count;
	if (param_count != b->param_count) {
		return false;
	}
	for (int i = 0; i < param_count; i++) {
		if (a->params[i] != b->params[i]) {
			return false;
		}
	}
	uint32_t var_count = a->var_count;
	if (var_count != b->var_count) {
		return false;
	}
	for (int i = 0; i < var_count; i++) {
		if (a->vars[i] != b->vars[i]) {
			return false;
		}
	}
	uint32_t term_count = a->term_count;
	if (term_count != b->term_count) {
		return false;
	}
	for (int i = 0; i < term_count; i++) {
		s_term_type type = a->terms[i].type;
		if (type != b->terms[i].type) {
			return false;
		}
		switch (type) {
			case LAMBDA:
				if (!s_eq_lambda(a->terms[i].lambda, b->terms[i].lambda)) {
					return false;
				}
				break;
			case VARIABLE:
				if (a->terms[i].variable != b->terms[i].variable) {
					return false;
				}
				break;
			default:
				if (!s_eq(a->terms[i].quote, b->terms[i].quote)) {
					return false;
				}
				break;
		}
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
		case VARIABLE:
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
		case VARIABLE:
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
		case VARIABLE:
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
			for (int i = 0; i < r->function.lambda->var_count; i++) {
				s_dealias(r->function.capture[i]);
			}
			s_free_lambda(r->function.lambda);
			break;
		case BUILTIN:
			free(r->builtin.name);
			r->builtin.free(r->builtin.data);
			break;
		case STRING:
			break;
		case BIG_INTEGER:
			break;
	}
	free(r);
}

s_term s_alias_term(s_term t) {
	switch (t.type) {
		case LAMBDA:
			s_ref_lambda(t.lambda);
			break;
		case VARIABLE:
			break;
		default:
			s_alias(t.quote);
			break;
	}
	return t;
}

void s_dealias_term(s_term t) {
	switch (t.type) {
		case LAMBDA:
			s_free_lambda(t.lambda);
			break;
		case VARIABLE:
			break;
		default:
			s_dealias(t.quote);
			break;
	}
}

s_lambda_term* s_ref_lambda(s_lambda_term* l) {
	atomic_fetch_add(&l->ref_count, 1);
}

void s_free_lambda(s_lambda_term* l) {
	if (atomic_fetch_add(&l->ref_count, -1) > 1) {
		if (l->param_count > 0) {
			for (int i = 0; i < l->param_count; i++) {
				s_free(SYMBOL, l->params[i]);
			}
			free(l->params);
		}
		if (l->var_count > 0) {
			free(l->vars);
			for (int i = 0; i < l->term_count; i++) {
				s_dealias_term(l->terms[i]);
			}
		}
		if (l->term_count > 0) {
			free(l->terms);
		}
	}
}

void s_init() {
	s_expr data = s_symbol(NULL, u_strref(u"data"));
	data_nil = s_symbol(data.p, u_strref(u"nil"));
	data_quote = s_symbol(data.p, u_strref(u"quote"));
	data_lambda = s_symbol(data.p, u_strref(u"lambda"));
	s_dealias(data);
}

void s_close() {
	idtrie_clear(table.trie);
}

