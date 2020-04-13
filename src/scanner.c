#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <uchar.h>
#include <unicode/utf.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>

#include "c-calipto/scanner.h"

int64_t input_position(scanner* s) {
	return s->input_cursor.position;
}

int64_t buffer_position(scanner* h) {
	return s->buffer_cursor.position;
}

void next_page(scanner* s) {
	page* p = s->input.page;
	if (p->next == NULL) {
		p->next = malloc(sizeof(page));
		p->next->block = s->stream.next_block(s->stream);
		p->next->next = NULL;
	}
}

cursor peek_input(scanner* s) {
	UChar c1 = *(s->input.pointer);
	if (U16_IS_LEAD(c1)) {
		UChar c2;
		if (s->input.pointer + 1 < s->input.page->block->end) {
	       		c2 = *(s->input.pointer + 1);
		} else {
			prepare_input(s);
			c2 = s->input.page->next->block->start;
		}
		if (U16_IS_TRAIL(c2)) {
			*c = U16_GET_SUPPLEMENTARY(c1, c2);
			return 2;
		} else {
			*c = 0xFFFD;
			return 0;
		}
    	} else {
		*c = c1;
		return 1;
	}
}

void next_buffer_page(scanner* s) {
	page* p = s->buffer_page;
	page* n = p->next;
	*s->buffer_cursor.page = n;
	s->buffer.clear_page(s->buffer, p);
	s->buffer_cursor.pointer = (n != NULL)
		? n->start
		: NULL;
}

int64_t advance_input_while(scanner* s, bool (*condition)(Char32)) {
	int64_t from = s->input_position;
	while (s->input_page != NULL) {
		while (s->input_pointer < s->input_page.end) {
			UChar c = *s->input_pointer;
			if (U16_IS_LEAD(c)) {
				if (s->input_pointer == s->input_page.end) {
					next_input_page(s);
				}
				UChar c2 = *(s->input_pointer++);
				if (U16_IS_TRAIL(c2)) {
					++i;
					c = U16_GET_SUPPLEMENTARY(c, c2);
				}
    			}
			if (condition(c)) {
				sh->input_pos++;
				sh->input += s;
			} else {
				return s->input_position - from;
			}
		}
		next_input_page(s);
	}
	return s->input_position - from;
}

bool advance_input_if(scanner* h, bool (*condition)(Char32)) {
	string_handle* sh = (string_handle*)(h + 1);
	if (*sh->input == U'\0') {
		return false;
	}
	Char32 c;
	int s = mbrtoc32(&c, sh->input, MB_LEN_MAX, NULL);
	if (condition(c)) {
		sh->input_pos++;
		sh->input += s;
		return true;
	} else {
		return false;
	}
}

bool advance_input_if_equal(scanner* h, Char32 c) {
	string_handle* sh = (string_handle*)(h + 1);
	if (*sh->input == U'\0') {
		return false;
	}
	Char32 c2;
	int s = mbrtoc32(&c2, sh->input, MB_LEN_MAX, NULL);
	if (c == c2) {
		sh->input_pos++;
		sh->input += s;
		return true;
	} else {
		return false;
	}
}

int64_t take_buffer_to(scanner* h, int64_t p, Char32* s) {
	string_handle* sh = (string_handle*)(h + 1);

	if (p > sh->input_pos) {
		p = sh->input_pos;
	}
	int64_t size = p - sh->buffer_pos;
	sh->buffer_pos = p;
	for (int j = 0; j < size; j++) {
		sh->buffer += mbrtoc32(s + j, sh->buffer, MB_CUR_MAX, NULL);
	}
	return size;
}

int64_t take_buffer_length(scanner* h, int64_t l, Char32* s) {
	return take_buffer_to(h, buffer_position(h) + l, s);
}

int64_t take_buffer(scanner* h, Char32* s) {
	return take_buffer_to(h, input_position(h), s);
}

int64_t discard_buffer_to(scanner* h, int64_t p) {
	string_handle* sh = (string_handle*)(h + 1);

	if (p > sh->input_pos) {
		p = sh->input_pos;
	}
	int64_t size = p - sh->buffer_pos;
	sh->buffer_pos = p;
	for (int j = 0; j < size; j++) {
		sh->buffer += mbrtoc32(NULL, sh->buffer, MB_CUR_MAX, NULL);
	}
	return size;
}

int64_t discard_buffer_length(scanner* h, int64_t l) {
	return discard_buffer_to(h, buffer_position(h) + l);
}

int64_t discard_buffer(scanner* h) {
	string_handle* sh = (string_handle*)(h + 1);
	int64_t size = sh->input_pos - sh->buffer_pos;
	sh->buffer = sh->input;
	sh->buffer_pos = sh->input_pos;
	return size;
}

void close_scanner(scanner* h) {
	free(h);
}

buffer* open_string_buffer(char* s) {
	scanner* h = malloc(sizeof(scanner) + sizeof(string_handle));
	h->vtable = vtable;
	string_handle* sh = (string_handle*)(h + 1);
	sh->input = s;
	sh->input_pos = 0;
	sh->buffer = s;
	sh->buffer_pos = 0;

	return h;
}

