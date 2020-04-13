#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>

#include "c-calipto/scanner.h"

int64_t input_position(scanner* s) {
	return s->input_position;
}

int64_t buffer_position(scanner* h) {
	return s->buffer_position;
}

void next_input_page(scanner* s) {
		page* p = s->input_page;
		*s->input_page = s->buffer.fill_page(s->buffer, s->input_page);
		*p->next = s->input_page;
}

void next_buffer_page(scanner* s) {
		page* p = s->buffer_page;
		*s->buffer_page = s->buffer_page->next;
		s->buffer.clear_page(s->buffer, p);
}

int64_t advance_input_while(scanner* s, bool (*condition)(char32_t)) {
	int64_t from = s->input_position;
	while (s->input_page != NULL) {
		while (s->input < s->input_page.end) {
			if (condition(s->
			char32_t c;
			int s = mbrtoc32(&c, sh->input, MB_LEN_MAX, NULL);
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

bool advance_input_if(scanner* h, bool (*condition)(char32_t)) {
	string_handle* sh = (string_handle*)(h + 1);
	if (*sh->input == U'\0') {
		return false;
	}
	char32_t c;
	int s = mbrtoc32(&c, sh->input, MB_LEN_MAX, NULL);
	if (condition(c)) {
		sh->input_pos++;
		sh->input += s;
		return true;
	} else {
		return false;
	}
}

bool advance_input_if_equal(scanner* h, char32_t c) {
	string_handle* sh = (string_handle*)(h + 1);
	if (*sh->input == U'\0') {
		return false;
	}
	char32_t c2;
	int s = mbrtoc32(&c2, sh->input, MB_LEN_MAX, NULL);
	if (c == c2) {
		sh->input_pos++;
		sh->input += s;
		return true;
	} else {
		return false;
	}
}

int64_t take_buffer_to(scanner* h, int64_t p, char32_t* s) {
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

int64_t take_buffer_length(scanner* h, int64_t l, char32_t* s) {
	return take_buffer_to(h, buffer_position(h) + l, s);
}

int64_t take_buffer(scanner* h, char32_t* s) {
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

