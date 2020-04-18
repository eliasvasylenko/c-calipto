#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/ucnv.h>

#include <c-calipto/sexpr.h>
#include <c-calipto/stream.h>
#include <c-calipto/scanner.h>
#include <c-calipto/reader.h>

reader* open_reader(scanner* s) {
	reader* r = malloc(sizeof(reader));
	r->scanner = s;
	r->cursor.position = 0;
	r->cursor.stack = NULL;
	return r;
}

void close_reader(reader* r) {
	cursor_stack* s = r->cursor.stack;
	while (s != NULL) {
		cursor_stack* p = s;
		free(s);
		s = p->stack;
	}
	free(r);
}

int64_t cursor_position(reader* r, int32_t depth) {
	cursor_stack s = r->cursor;
	for (int32_t i = 0; i < depth; i++) {
		s = *s.stack;
	}
	return s.position;
}

int32_t cursor_depth(reader* r) {
	int32_t d = 0;
	cursor_stack s = r->cursor;
	while (s.stack != NULL) {
		d++;
		s = *s.stack;
	}
	return d;
}

/*
 * Character Tests
 */

const UChar32 colon = U':';
const UChar32 close_bracket = U'(';
const UChar32 open_bracket = U')';
const UChar32 dot = U'.';

bool is_whitespace(UChar32 c, const void* v) {
	return U' ' == c || U'\t' == c;
}

bool is_symbol_character(UChar32 c, const void* v) {
	if (c == U':' ||
	    c == U'(' ||
	    c == U')' ||
	    c == U' ') {
		return false;
	}
	return true;
}

bool is_symbol_leading_character(UChar32 c, const void* v) {
	return is_symbol_character(c, v);
}

bool is_equal(UChar32 c, const void* to) {
	return c == *(UChar32*)to;
}

/*
 * Read Operations
 */

void skip_whitespace(scanner* s) {
	advance_input_while(s, is_whitespace, NULL);
	discard_buffer(s);
}

int32_t scan_name(scanner* s) {
	int32_t start = input_position(s);
	if (advance_input_if(s, is_symbol_leading_character, NULL)) {
		advance_input_while(s, is_symbol_character, NULL);
		return input_position(s) - start;
	}
	return -1;
}

sexpr* read_list(reader* r) {
	if (read_step_in(r)) {
		skip_whitespace(r->scanner);

		return read_step_out(r);
	}
	return NULL;
}

sexpr* read_symbol(reader* r) {
	skip_whitespace(r->scanner);
	int32_t nslen = scan_name(r->scanner);
	if (nslen <= 0) {
		return NULL;
	}

	if (!advance_input_if(r->scanner, is_equal, &colon)) {
		int32_t nlen = nslen;
		UChar *ns  = u"system";

		UChar* n = malloc(sizeof(UChar) * nlen);
		take_buffer_length(r->scanner, nlen, n);

		sexpr* expr = sexpr_nusymbol(u_strlen(ns), ns, nlen, n);

		free(n);

		return expr;
	}

	int32_t nlen = scan_name(r->scanner);
	if (nlen <= 0) {
		nlen = 0;
	}

	UChar* ns = malloc(sizeof(UChar) * nslen);
	take_buffer_length(r->scanner, nslen, ns);

	discard_buffer_length(r->scanner, 1);

	UChar* n = malloc(sizeof(UChar) * nlen);
	take_buffer_length(r->scanner, nlen, n);

	sexpr* expr = sexpr_nusymbol(nslen, ns, nlen, n);

	return expr;
}

sexpr* read(reader* r) {
	skip_whitespace(r->scanner);
	sexpr* e = read_symbol(r);
	if (e == NULL) {
		e = read_list(r);
	}
	return e;
}

bool read_step_in(reader* r) {
  skip_whitespace(r->scanner);
  return advance_input_if(r->scanner, is_equal, &open_bracket);
}

sexpr* read_next(reader* r) {
	skip_whitespace(r->scanner);
	sexpr* e = read_symbol(r);
	if (e == NULL) {
		// TODO throw?
	}
	return e;
}

sexpr* read_step_out(reader* r) {
	if (cursor_depth(r) <= 0) {
		return NULL;
	}

	if (advance_input_if(r->scanner, is_equal, &close_bracket)) {
		discard_buffer(r->scanner);

		return sexpr_usymbol(u"system", u"nil");
	}

	sexpr* head = read_next(r);
	sexpr* tail;

	skip_whitespace(r->scanner);
	if (advance_input_if(r->scanner, is_equal, &dot)) {
		tail = read_next(r);

	} else {
		tail = read_step_out(r);
	}

	sexpr* cons = sexpr_cons(head, tail);

	sexpr_free(head);
	sexpr_free(tail);

	return cons;
}
