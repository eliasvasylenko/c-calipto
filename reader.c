#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>

#include <sexpr.h>
#include <scanner.h>
#include <reader.h>

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

bool is_whitespace(char32_t c) {
	return U' ' == c || U'\t' == c;
}

bool is_symbol_character(char32_t c) {
	;
}

bool is_symbol_leading_character(char32_t c) {
	;
}

bool is_namespace_separator(char32_t c) {
	return c == U':';
}

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

}

sexpr* read_symbol(reader_handle* r) {
	skip_whitespace(r->scanner);
	int32_t nslen = scan_name(r->scanner);
	if (nslen > 0) {
		return NULL;
	}

	if (!advance_input_if(r->scanner, is_namespace_separator)) {
		// TODO throw?
	}

	int32_t nlen = scan_name(r->scanner);
	if (nlen < 0) {
		// TODO throw?
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

}

sexpr* read_step_out(reader_handle* r) {

}

/*
 * Make our life easier here!
 *
 * We don't need most of the features of the reader just to load the bootstrap code.
 *
 * Just expose a single "read" routine which loads in an entire root s-expression.
 *
 * We also don't need most of the features of the _scanner_; we can limit the bootstrap
 * code to single-byte codepoints in utf8.
 *
 * Both the scanner AND the reader are ONLY present to load the bootstrap code! Once that's
 * loaded it should contain a new implementation of the scanner and reader.
 */
