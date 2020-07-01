#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/ucnv.h>
#include <unicode/ustdio.h>

#include "c-ohvu/io/stringref.h"
#include "c-ohvu/io/stream.h"
#include "c-ohvu/io/scanner.h"

#include "c-ohvu/data/bdtrie.h"
#include "c-ohvu/data/sexpr.h"
#include "c-ohvu/data/reader.h"

typedef ovio_scanner scanner;
typedef ovio_strref strref;
typedef ovda_reader reader;
typedef ovda_cursor_stack cursor_stack;
typedef ovs_expr expr;

reader* ovda_open_reader(scanner* s) {
	reader* r = malloc(sizeof(reader));
	r->scanner = s;
	r->cursor.position = 0;
	r->cursor.stack = NULL;

	expr data = ovs_symbol(NULL, ovio_u_strref(u"data"));
	r->data_quote = ovs_symbol(data.p, ovio_u_strref(u"quote"));
	r->data_nil = ovs_symbol(data.p, ovio_u_strref(u"nil"));
	ovs_dealias(data);

	return r;
}

void ovda_close_reader(reader* r) {
	cursor_stack* s = r->cursor.stack;
	while (s != NULL) {
		cursor_stack* p = s;
		s = p->stack;
		free(p);
	}
	ovs_dealias(r->data_quote);
	ovs_dealias(r->data_nil);
	free(r);
}

int64_t ovda_cursor_position(reader* r, int32_t depth) {
	cursor_stack s = r->cursor;
	for (int32_t i = 0; i < depth; i++) {
		s = *s.stack;
	}
	return s.position;
}

int32_t ovda_cursor_depth(reader* r) {
	int32_t d = 0;
	cursor_stack s = r->cursor;
	while (s.stack != NULL) {
		d++;
		s = *s.stack;
	}
	return d;
}

int32_t push_cursor(reader* r) {
	cursor_stack* s = malloc(sizeof(cursor_stack));
	*s = r->cursor;
	r->cursor.position = 0;
	r->cursor.stack = s;
}

int32_t pop_cursor(reader* r) {
	cursor_stack* s = r->cursor.stack;
	r->cursor = *s;
	free(s);
}

/*
 * Character Tests
 */

const UChar32 colon = U':';
const UChar32 open_bracket = U'(';
const UChar32 close_bracket = U')';
const UChar32 dot = U'.';
const UChar32 double_quote = U'"';
const UChar32 single_quote = U'\'';

bool is_whitespace(UChar32 c, const void* v) {
	return U' ' == c || U'\t' == c || U'\n' == c;
}

bool isymbol_character(UChar32 c, const void* v) {
	if (c == U':' ||
	    c == U'(' ||
	    c == U')' ||
	    is_whitespace(c, v)) {
		return false;
	}
	return true;
}

bool isymbol_leading_character(UChar32 c, const void* v) {
	return isymbol_character(c, v);
}

bool is_equal(UChar32 c, const void* to) {
	return c == *(UChar32*)to;
}

bool is_not_equal(UChar32 c, const void* to) {
	return c != *(UChar32*)to;
}

/*
 * Read Operations
 */

void skip_whitespace(scanner* s) {
	ovio_advance_input_while(s, is_whitespace, NULL);
	ovio_discard_buffer(s);
}

int32_t scan_name(scanner* s) {
	int32_t start = ovio_input_position(s);
	if (ovio_advance_input_if(s, isymbol_leading_character, NULL)) {
		ovio_advance_input_while(s, isymbol_character, NULL);
		return ovio_input_position(s) - start;
	}
	return -1;
}

ovda_result ovda_read_list(reader* r, expr* e) {
	if (ovda_read_step_in(r) == OVDA_SUCCESS) {
		skip_whitespace(r->scanner);

		return ovda_read_step_out(r, e);
	}
	return OVDA_SUCCESS;
}

ovda_result ovda_read_symbol(reader* r, expr* e) {
	skip_whitespace(r->scanner);
       
	expr symbol = { OVS_SYMBOL, .p=NULL };

	do {
		ovio_discard_buffer(r->scanner);
		int32_t len = scan_name(r->scanner);
		if (len <= 0) {
			if (symbol.p) {
				return OVDA_INVALID;
			}
			return OVDA_UNEXPECTED_TYPE;
		}
		UChar* n = malloc(sizeof(UChar) * len);
		ovio_take_buffer_length(r->scanner, len, n);

		symbol = ovs_symbol(symbol.p, ovio_u_strnref(len, n));

		free(n);
	} while (ovio_advance_input_if(r->scanner, is_equal, &colon));

	*e = symbol;

	return OVDA_SUCCESS;
}

ovda_result read_string(reader* r, expr* e) {
	if (!ovio_advance_input_if(r->scanner, is_equal, &double_quote)) {
		return OVDA_UNEXPECTED_TYPE;
	}

	ovio_discard_buffer(r->scanner);
	ovio_advance_input_while(r->scanner, is_not_equal, &double_quote);

	int32_t len = ovio_input_position(r->scanner) - ovio_buffer_position(r->scanner);

	if (!ovio_advance_input_if(r->scanner, is_equal, &double_quote)) {
		return OVDA_INVALID;
	}

	UChar* c = malloc(sizeof(UChar) * len);
	ovio_take_buffer_length(r->scanner, len, c);
	expr string = ovs_string(ovio_u_strnref(len, c));

	expr list[] = { r->data_quote, string };
	*e = ovs_list(2, list);

	ovs_dealias(string);
	free(c);

	return OVDA_SUCCESS;
}

ovda_result read_quote(reader* r, expr* e) {
	if (!ovio_advance_input_if(r->scanner, is_equal, &single_quote)) {
		return OVDA_UNEXPECTED_TYPE;
	}

	ovio_discard_buffer(r->scanner);

	expr data;
	if (ovda_read(r, &data) != OVDA_SUCCESS) {
		return OVDA_INVALID;
	}

	expr list[] = { r->data_quote, data };
	*e = ovs_list(2, list);

	ovs_dealias(data);

	return OVDA_SUCCESS;
}

ovda_result read_next(reader* r, expr* e) {
	ovda_result res;
	res = read_string(r, e);

	if (res == OVDA_UNEXPECTED_TYPE) {
		res = read_quote(r, e);

		if (res == OVDA_UNEXPECTED_TYPE) {
			res = ovda_read_symbol(r, e);

			if (res == OVDA_UNEXPECTED_TYPE) {
				res = ovda_read_list(r, e);
			}
		}
	}
	return res;
}

ovda_result ovda_read(reader* r, expr* e) {
	skip_whitespace(r->scanner);
	ovda_result res = read_next(r, e);
	if (res != OVDA_SUCCESS) {
		res = OVDA_INVALID;
	}
	return res;
}

ovda_result ovda_read_step_in(reader* r) {
	skip_whitespace(r->scanner);
	if (!ovio_advance_input_if(r->scanner, is_equal, &open_bracket)) {
		return OVDA_UNEXPECTED_TYPE;
	}
	push_cursor(r);
	return OVDA_SUCCESS;
}

ovda_result ovda_read_step_out(reader* r, expr* e) {
	if (ovda_cursor_depth(r) <= 0) {
		return OVDA_UNEXPECTED_TYPE;
	}

	if (ovio_advance_input_if(r->scanner, is_equal, &close_bracket)) {
		ovio_discard_buffer(r->scanner);

		*e = ovs_alias(r->data_nil);
		return OVDA_SUCCESS;
	}

	expr head;
	expr tail;
	if (read_next(r, &head) != OVDA_SUCCESS) {
		return OVDA_INVALID;
	}

	skip_whitespace(r->scanner);
	if (ovio_advance_input_if(r->scanner, is_equal, &dot)) {
		if (!read_next(r, &tail)) {
			ovs_dealias(head);
			return OVDA_INVALID;
		}

		skip_whitespace(r->scanner);
		if (!ovio_advance_input_if(r->scanner, is_equal, &close_bracket)) {
			ovs_dealias(head);
			return OVDA_INVALID;
		}
		
	} else if (ovda_read_step_out(r, &tail) != OVDA_SUCCESS) {
		ovs_dealias(head);
		return OVDA_INVALID;
	}

	expr cons = ovs_cons(head, tail);

	ovs_dealias(head);
	ovs_dealias(tail);

	*e = cons;
	return OVDA_SUCCESS;
}
