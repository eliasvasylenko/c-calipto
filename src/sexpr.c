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
	s_expr_ref* r = ref(sizeof(s_expr));
	r->quote = e;
	s_alias(e);
	return (s_expr){ QUOTE, .p=r };
}

s_expr s_lambda(int32_t param_count, int32_t var_count, s_expr_ref** vars,
		int32_t term_count, s_expr* terms) {
	s_expr_ref* r = ref(sizeof(s_lambda_data));
	r->lambda.param_count = param_count;
	r->lambda.var_count = var_count;
	if (var_count > 0) {
		r->lambda.vars = malloc(sizeof(s_expr_ref*) * var_count);
		for (int i = 0; i < var_count; i++) {
			s_ref(vars[i]);
			r->lambda.vars[i] = vars[i];
		}
	} else {
		r->lambda.vars = NULL;
	}
	if (term_count > 0) {
		r->lambda.terms = malloc(sizeof(s_expr) * term_count);
		for (int i = 0; i < term_count; i++) {
			s_alias(terms[i]);
			r->lambda.terms[i] = terms[i];
		}
	} else {
		r->lambda.terms = NULL;
	}
	return (s_expr){ LAMBDA, .p=r };
}

s_expr s_car(const s_expr e) {
	switch (e.type) {
		case CONS:
			s_alias(e.p->cons.car);
			return e.p->cons.car;

		case QUOTE:
			s_alias(data_quote);
			return data_quote;

		case LAMBDA:
			s_alias(data_lambda);
			return data_lambda;

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

		case QUOTE:
			return s_cons(e.p->quote, data_nil);

		case LAMBDA:
			;
			s_expr params = data_nil;
			for (int i = e.p->lambda.param_count - 1; i >= 0; i--) {
				s_expr tail = params;
				params = s_cons(e.lambda->params[i], tail);
				s_free(tail);
			}
			s_expr tail = s_cons(e.lambda->body, s_nil());
			s_expr cdr = s_cons(params, tail);
			s_free(params);
			s_free(tail);
			return cdr;

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
		
		case STATEMENT:
			;
			s_expr args = s_nil();
			for (int i = e.statement->arg_count - 1; i >= 0; i--) {
				s_expr tail = args;
				args = s_cons(e.statement->args[i], tail);
				s_free(tail);
			}
			return args;

		default:
			return s_error(u_strref(u"Type error, cannot destructure atom"));
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
		case CONS:
			return s_eq(a.cons->car, b.cons->car);
		case SYMBOL:
			return a.p == b.p;
		case STRING:
		case CHARACTER:
		case INTEGER:
			break;
		case NIL:
			return true;
	}
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
			s_free(e.p);
	}
}

s_expr_ref* s_ref(s_expr_ref* r) {
	if (e.ref_count != NULL) {
		atomic_fetch_add(e.ref_count, 1);
	}
	return e;
}

void s_free(s_expr_type t, s_expr_ref* r) {
	if (e.ref_count == NULL || atomic_fetch_add(e.ref_count, -1) > 1) {
		return;
	}
	switch (e.type) {
		case CHARACTER:
		case INTEGER:
		case VARIABLE:
			return;
		case ERROR:
			break;
		case SYMBOL:
			idtrie_remove(e.p->symbol);
			break;
		case CONS:
			s_dealias(e.p->cons.car);
			s_dealias(e.p->cons.cdr);
			break;
		case QUOTE:
			s_dealias(*e.p->quote);
			break;
		case LAMBDA:
			if (e.p->lambda.var_count > 0) {
				for (int j = 0; j < e.p->lambda.var_count; j++) {
					s_dealias(e.p->lambda.vars[j]);
				}
				free(e.p->lambda.vars);
			}
			if (e.p->lambda.term_count > 0) {
				for (int j = 0; j < e.p->lambda.term_count; j++) {
					s_dealias(e.p->lambda.terms[j]);
				}
				free(e.p->lambda.terms);
			}
			break;
		case FUNCTION:
			s_dealias(e.function->capture);
			s_dealias(e.function->lambda);
			break;
		case BUILTIN:
			free(e.builtin->name);
			e.builtin->free(e.builtin->data);
			break;
		case STRING:
			break;
		case BIG_INTEGER:
			break;
	}
	free(e.p);
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
	atomic_fetch_add(lambda->ref_count, 1);
}

void s_free_lambda(s_lambda_term* l) {
	if (atomic_fetch_add(lambda->ref_count, -1) > 1) {
		for (int i = 0; i < lambda->param_count; i++) {
			s_free(lambda->params[i]);
		}
		free(lambda->params);
		free(lambda->vars);
		for (int i = 0; i < lambda->term_count; i++) {
			s_dealias_term(lambda->terms[i]);
		}
		free(lambda->terms);
	}
}

void s_init() {
	s_expr data = s_intern(NULL, u_strref(u"data"));
	data_nil = s_intern(data, u_strref(u"nil"));
	data_quote = s_intern(data, u_strref(u"quote"));
	data_lambda = s_intern(data, u_strref(u"lambda"));
}

void s_close() {
	idtrie_clear(table.trie);
}

