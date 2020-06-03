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

s_expr_ref* ref(int32_t payload_size) {
	s_expr_ref* r = malloc(sizeof(_Atomic(uint32_t)) + payload_size);
	r->ref_count = ATOMIC_VAR_INIT(1);
	return r;
}

s_expr s_error() {
	return (s_expr){ ERROR, .p=NULL };
}

s_expr s_function(s_expr_ref* lambda, s_expr* capture) {
	s_expr_ref* r = ref(sizeof(s_function_data));
	r->function.lambda = lambda;
	r->function.capture = capture;

	s_ref(lambda);

	int32_t capture_count = lambda->lambda.var_count - lambda->lambda.param_count;
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

s_table s_init_table() {
	s_table t;
	t.trie.root;

	// TODO insert common symbols, e.g. nil

	return t;
}

typedef struct s_key {
	s_expr_ref* qualifier;
	UChar name[1];
} s_key;

void* s_value(void* key, id id) {
	s_expr_ref* r = ref(sizeof(s_symbol_data));
	r->symbol.id = id;
	return r;
}

s_expr_ref* s_intern(s_table t, s_expr_ref* qualifier, strref name) {
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
			t.trie,
			sizeof(s_key) + sizeof(UChar) * (len - 1),
			key,
			s_value);

	return (s_expr_ref*)id.leaf->value;
}

const UChar const* unicode_ns = u"unicode";
const int32_t unicode_nsl = 7;

s_expr s_symbol(s_table t, s_expr_ref* q, strref n) {
	return (s_expr){ SYMBOL, .p=s_intern(t, q, n) };
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

s_expr s_quote(s_expr e) {
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

/*
 * We don't want to have to allocate memory here when the string already exists, but
 * then when we have to materialize a string there will be nobody to free it.
 *
 * TODO We must allocate strings upon request and put them into the interning table,
 * then we can free them when the refcount hits 0.
 */
UChar* s_name(const s_expr e) {
	switch (e.type) {
		case ERROR:
			return e.error;

		case SYMBOL:
			return e.symbol->name;

		case NIL:
			return u"nil";

		case BUILTIN:
			return e.builtin->name;

		case FUNCTION:
			return u"_";

		case CHARACTER:
			return u"_char_";
		
		default:
			return NULL;
	}
}

UChar* s_namespace(const s_expr e) {
	switch (e.type) {
		case ERROR:
			return u"error";

		case SYMBOL:
			return e.symbol->namespace;

		case NIL:
			return u"data";

		case BUILTIN:
			return u"builtin";

		case FUNCTION:
			return u"function";

		case CHARACTER:
			return u"unicode";
		
		default:
			return NULL;
	}
}

static s_expr data_quote = { ERROR, NULL, .error=NULL };
static s_expr data_lambda = { ERROR, NULL, .error=NULL };

s_expr s_car(const s_expr e) {
	switch (e.type) {
		case CONS:
			s_ref(e.cons->car);
			return e.cons->car;

		case QUOTE:
			if (data_quote.type == ERROR) {
				data_quote = s_symbol(u_strref(u"data"), u_strref(u"quote"));
			}
			s_ref(data_quote);
			return data_quote;

		case LAMBDA:
			if (data_lambda.type == ERROR) {
				data_lambda = s_symbol(u_strref(u"data"), u_strref(u"lambda"));
			}
			s_ref(data_lambda);
			return data_lambda;

		case STRING:
			;
			bool single = U16_IS_SINGLE(e.string[0]);
			UChar32 cp = single
				? e.string[0]
				: U16_GET_SUPPLEMENTARY(e.string[0], e.string[1]);
			return s_character(cp);

		case STATEMENT:
			;
			s_ref(e.statement->target);
			return e.statement->target;

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
			s_ref(e.cons->cdr);
			return e.cons->cdr;

		case QUOTE:
			return s_cons(*e.quote, s_nil());

		case LAMBDA:
			;
			s_expr params = s_nil();
			for (int i = e.lambda->param_count - 1; i >= 0; i--) {
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

void s_alias(const s_expr e) {
	s_expr me = (s_expr) e;
	if (me.ref_count != NULL) {
		atomic_fetch_add(me.ref_count, 1);
	}
}

void s_dealias(s_expr e) {
	if (e.ref_count == NULL || atomic_fetch_add(e.ref_count, -1) > 1) {
		return;
	}
	switch (e.type) {
		case ERROR:
			free(e.error);
			break;
		case SYMBOL:
			free(e.symbol->name);
			free(e.symbol->namespace);
			free(e.symbol);
			break;
		case CONS:
			s_dealias(e.cons->car);
			s_dealias(e.cons->cdr);
			free(e.cons);
			break;
		case NIL:
			break;
		case BUILTIN:
			free(e.builtin->name);
			e.builtin->free(e.builtin->data);
			free(e.builtin);
			break;
		case FUNCTION:
			s_dealias(e.function->capture);
			s_dealias(e.function->lambda);
			free(e.function);
			break;
		case QUOTE:
			s_dealias(*e.quote);
			free(e.quote);
			break;
		case LAMBDA:
			if (e.lambda->param_count > 0) {
				for (int j = 0; j < e.lambda->param_count; j++) {
					s_dealias(e.lambda->params[j]);
				}
				free(e.lambda->params);
			}
			s_dealias(e.lambda->body);
			free(e.lambda);
			break;
		case STATEMENT:
			if (e.statement->free_var_count > 0) {
				for (int i = 0; i < e.statement->free_var_count; i++) {
					s_dealias(e.statement->free_vars[i]);
				}
				free(e.statement->free_vars);
			}
			s_free(e.statement->target);
			if (e.statement->arg_count > 0) {
				for (int i = 0; i < e.statement->arg_count; i++) {
					s_dealias(e.statement->args[i]);
				}
				free(e.statement->args);
			}
			free(e.statement);
			break;
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
