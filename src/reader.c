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

#include "c-calipto/sexpr.h"
#include "c-calipto/stream.h"
#include "c-calipto/scanner.h"
#include "c-calipto/reader.h"

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
		s = p->stack;
		free(p);
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

bool is_whitespace(UChar32 c, const void* v) {
	return U' ' == c || U'\t' == c;
}

bool is_symbol_character(UChar32 c, const void* v) {
	if (c == U':' ||
	    c == U'(' ||
	    c == U')' ||
	    is_whitespace(c, v)) {
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

s_expr read_list(reader* r) {
	if (read_step_in(r)) {
		skip_whitespace(r->scanner);

		return read_step_out(r);
	}
	return s_alloc_error("Failed to scan list open");
}

s_expr read_symbol(reader* r) {
	skip_whitespace(r->scanner);
	int32_t nslen = scan_name(r->scanner);
	if (nslen <= 0) {
		return s_alloc_error("Failed to scan name");
	}

	if (!advance_input_if(r->scanner, is_equal, &colon)) {
		int32_t nlen = nslen;
		UChar *ns  = u"bootstrap";

		UChar* n = malloc(sizeof(UChar) * nlen);
		take_buffer_length(r->scanner, nlen, n);

		s_symbol sym = { s_u_strcpy(ns), s_u_strncpy(nlen, n) };
		s_expr expr = s_alloc_symbol(sym);

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

	s_symbol sym = { s_u_strncpy(nslen, ns), s_u_strncpy(nlen, n) };
	s_expr expr = s_alloc_symbol(sym);

	free(ns);
	free(n);

	return expr;
}

s_expr read_next(reader* r) {
	s_expr e = read_symbol(r);
	if (e.type == ERROR) {
		e = read_list(r);
	}
	return e;
}

s_expr read(reader* r) {
	skip_whitespace(r->scanner);
	return read_next(r);
}

bool read_step_in(reader* r) {
  skip_whitespace(r->scanner);
  bool success = advance_input_if(r->scanner, is_equal, &open_bracket);
  if (success) {
	  push_cursor(r);
  }
  return success;
}

s_expr read_step_out(reader* r) {
	if (cursor_depth(r) <= 0) {
		return s_alloc_error("Unexpected list terminator");
	}

	if (advance_input_if(r->scanner, is_equal, &close_bracket)) {
		discard_buffer(r->scanner);

		return s_alloc_nil();
	}

	s_expr head = read_next(r);
	s_expr tail;

	skip_whitespace(r->scanner);
	if (advance_input_if(r->scanner, is_equal, &dot)) {
		tail = read_next(r);

		skip_whitespace(r->scanner);
		s_expr nil = read_step_out(r);
		s_expr_type t = nil.type;
		s_free(nil);
		if (t != NIL) {
			s_expr_free(head);

			return s_alloc_error("Illegal list terminator");
		}
	} else {
		tail = read_step_out(r);
	}

	s_cons c = { head, tail };
	s_expr cons = s_alloc_cons(c);

	s_free(head);
	s_free(tail);

	return cons;
}
