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

scanner* open_scanner(stream* s) {
	scanner* sc = malloc(sizeof(scanner));
	
	sc->next.page = NULL;
	sc->input.page = NULL;
	sc->buffer.page = NULL;

	sc->next.address = NULL;
	sc->input.address = NULL;
	sc->buffer.address = NULL;

	sc->next.size = 0;
	sc->input.size = 0;
	sc->buffer.size = 0;

	sc->next.position = 0;
	sc->input.position = 0;
	sc->buffer.position = 0;

	sc->next_character = U'\0';
	
	return sc;
}

int64_t input_position(scanner* s) {
	return s->input.position;
}

int64_t buffer_position(scanner* s) {
	return s->buffer.position;
}

void prepare_next_address(scanner* s) {
	if (s->next.page == NULL) {
		s->next.page = malloc(sizeof(page));
		s->next.page->block = s->stream->next_block(s->stream);
		s->next.page->next = NULL;
		if (s->next.page->block != NULL) {
			s->next.address = s->next.page->block->start;
		}
		if (s->input.page != NULL) {
			s->input.page->next = s->next.page;
		}
	}
}

UChar advance_next_address(scanner* s) {
	if (s->next.address == NULL) {
		return EOF;
	}
	UChar c = *s->next.address;
	s->next.size++;
	s->next.address++;
	if (s->next.address == s->next.page->block->end) {
		s->next.page = NULL;
		s->next.address = NULL;
	}
}

void prepare_next_character(scanner* s) {
	if (s->next.position != s->input.position) {
		return;
	}
	s->next.position++;

	prepare_next_address(s);
	if (s->next.address == NULL) {
		s->next_character = EOS;
		return;
	}

	UChar c1 = advance_next_address(s);
	if (U16_IS_TRAIL(c1)) {
		s->next_character = MALFORMED;
		return;
	}
	if (!U16_IS_LEAD(c1)) {
		s->next_character = c1;
		return;
	}

	prepare_next_address(s);
	if (s->next.address == NULL) {
		s->next_character = MALFORMED;
		return;
	}

	UChar c2 = advance_next_address(s);
	s->next_character = U16_GET_SUPPLEMENTARY(c1, c2);
}

void advance_next_character(scanner* s) {
	s->input = s->next;
}

int64_t advance_input_while(scanner* s, bool (*condition)(UChar32 c, void* v), void* context) {
	int64_t from = s->input.size;
	prepare_next_character(s);
	while (s->next_character != EOS &&
			s->next_character != MALFORMED &&
			condition(s->next_character, context)) {
		advance_next_character(s);
		prepare_next_character(s);
	}
	return s->input.size - from;
}

bool advance_input_if(scanner* s, bool (*condition)(UChar32 c, void* v), void* context) {
	prepare_next_character(s);
	if (s->next_character != EOS &&
			s->next_character != MALFORMED &&
			condition(s->next_character, context)) {
		advance_next_character(s);
		return true;
	} else {
		return false;
	}
}

int64_t take_buffer_to(scanner* h, int64_t p, UChar* s) {
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

int64_t take_buffer_length(scanner* h, int64_t l, UChar* s) {
	return take_buffer_to(h, buffer_position(h) + l, s);
}

int64_t take_buffer(scanner* h, UChar* s) {
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

