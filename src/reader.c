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

#include "c-calipto/stringref.h"
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

bool read_list(reader* r, s_expr* e) {
	if (read_step_in(r)) {
		skip_whitespace(r->scanner);

		return read_step_out(r, e);
	}
	return false;
}

bool read_symbol(reader* r, s_expr* e) {
	skip_whitespace(r->scanner);
	int32_t nslen = scan_name(r->scanner);
	if (nslen <= 0) {
		return false;
	}

	if (!advance_input_if(r->scanner, is_equal, &colon)) {
		int32_t nlen = nslen;
		UChar *ns  = u"bootstrap";

		UChar* n = malloc(sizeof(UChar) * nlen);
		take_buffer_length(r->scanner, nlen, n);

		*e = s_symbol(u_strref(ns), u_strnref(nlen, n));

		free(n);

		return true;
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

	*e = s_symbol(u_strnref(nslen, ns), u_strnref(nlen, n));

	free(ns);
	free(n);

	return true;
}

bool read_next(reader* r, s_expr* e) {
	return read_symbol(r, e) || read_list(r, e);
}

bool read(reader* r, s_expr* e) {
	skip_whitespace(r->scanner);
	return read_next(r, e);
}

bool read_step_in(reader* r) {
  skip_whitespace(r->scanner);
  bool success = advance_input_if(r->scanner, is_equal, &open_bracket);
  if (success) {
	  push_cursor(r);
  }
  return success;
}

bool read_step_out(reader* r, s_expr* e) {
	if (cursor_depth(r) <= 0) {
		return false;
	}

	if (advance_input_if(r->scanner, is_equal, &close_bracket)) {
		discard_buffer(r->scanner);

		*e = s_nil();
		return true;
	}

	s_expr head;
	s_expr tail;
	if (!read_next(r, &head)) {
		return false;
	}

	skip_whitespace(r->scanner);
	if (advance_input_if(r->scanner, is_equal, &dot)) {
		if (!read_next(r, &tail)) {
			s_free(head);
			return false;
		}

		skip_whitespace(r->scanner);
		s_expr nil;
		if (!read_step_out(r, &nil)) {
			s_free(head);
			return false;
		}
		s_expr_type t = nil.type;
		s_free(nil);
		if (t != NIL) {
			s_free(head);
			return false;
		}
	} else if (!read_step_out(r, &tail)) {
		s_free(head);
		return false;
	}

	s_expr cons = s_cons(head, tail);

	s_free(head);
	s_free(tail);

	*e = cons;
	return true;
}
