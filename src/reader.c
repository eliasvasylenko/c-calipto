#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>
#include <string.h>

#include <c-calipto/sexpr.h>
#include <c-calipto/scanner.h>
#include <c-calipto/reader.h>

reader_handle* open_reader(scanner_handle* s) {
	reader_handle* r = malloc(sizeof(reader_handle));
	r->scanner = s;
	r->cursor.position = 0;
	r->cursor.stack = NULL;
	return r;
}

void close_reader(reader_handle* r) {
	cursor_stack* s = r->cursor.stack;
	while (s != NULL) {
		cursor_stack* p = s;
		free(s);
		s = p->stack;
	}
	free(r);
}

int64_t cursor_position(reader_handle* r, int32_t depth) {
	cursor_stack s = r->cursor;
	for (int32_t i = 0; i < depth; i++) {
		s = *s.stack;
	}
	return s.position;
}

int32_t cursor_depth(reader_handle* r) {
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

bool is_whitespace(char32_t c) {
	return U' ' == c || U'\t' == c;
}

bool is_symbol_character(char32_t c) {
	if (c == U':' ||
	    c == U'(' ||
	    c == U')' ||
	    c == U' ') {
		return false;
	}
	return true;
}

bool is_symbol_leading_character(char32_t c) {
	return is_symbol_character(c);
}

/*
 * Read Operations
 */

void skip_whitespace(scanner_handle* s) {
	advance_input_while(s, is_whitespace);
	discard_buffer(s);
}

int32_t scan_name(scanner_handle* s) {
	int32_t start = input_position(s);
	if (advance_input_if(s, is_symbol_leading_character)) {
		advance_input_while(s, is_symbol_character);
		return input_position(s) - start;
	}
	return -1;
}

sexpr* read_list(reader_handle* r) {
	if (read_step_in(r)) {
		skip_whitespace(r->scanner);

		return read_step_out(r);
	}
	return NULL;
}

sexpr* read_symbol(reader_handle* r) {
	skip_whitespace(r->scanner);
	int32_t nslen = scan_name(r->scanner);
	if (nslen <= 0) {
		return NULL;
	}

	if (!advance_input_if_equal(r->scanner, U':')) {
		int32_t nlen = nslen;
		char32_t *ns  = U"system";
		nslen = 0;
		while (ns[nslen] != U'\0') nslen++;

		sexpr* expr = sexpr_empty_symbol(nslen, nlen);
		char32_t* payload = (char32_t*)(expr + 1);

		memcpy(payload, ns, nslen * sizeof(char32_t));
		take_buffer_length(r->scanner, nslen, payload + 7);

		return expr;
	}

	int32_t nlen = scan_name(r->scanner);
	if (nlen <= 0) {
		nlen = 0;
	}

	sexpr* expr = sexpr_empty_symbol(nslen, nlen);
	char32_t* payload = (char32_t*)(expr + 1);

	take_buffer_length(r->scanner, nslen, payload);
	discard_buffer_length(r->scanner, 1);
	take_buffer_length(r->scanner, nlen, payload + nslen + 1);

	return expr;
}

sexpr* read(reader_handle* r) {
	skip_whitespace(r->scanner);
	sexpr* e = read_symbol(r);
	if (e == NULL) {
		e = read_list(r);
	}
	return e;
}

bool read_step_in(reader_handle* r) {
  skip_whitespace(r->scanner);
  return advance_input_if_equal(r->scanner, U'(');
}

sexpr* read_next(reader_handle* r) {
	skip_whitespace(r->scanner);
	sexpr* e = read_symbol(r);
	if (e == NULL) {
		// TODO throw?
	}
	return e;
}

sexpr* read_step_out(reader_handle* r) {
	if (cursor_depth(r) <= 0) {
		return NULL;
	}

	if (advance_input_if_equal(r->scanner, U')')) {
		discard_buffer(r->scanner);

		return sexpr_symbol(U"system", U"nil");
	}

	sexpr* head = read_next(r);
	sexpr* tail;

	skip_whitespace(r->scanner);
	if (advance_input_if_equal(r->scanner, U'.')) {
		tail = read_next(r);

	} else {
		tail = read_step_out(r);
	}

	sexpr* cons = sexpr_cons(head, tail);

	sexpr_free(head);
	sexpr_free(tail);

	return cons;
}
