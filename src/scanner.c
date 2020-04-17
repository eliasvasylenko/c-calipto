#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
		} else {
			s->input.page = s->next.page;
			s->input.address = s->input.page->block->start;
			s->buffer.page = s->next.page;
			s->buffer.address = s->buffer.page->block->start;
		}
	}
}

UChar advance_next_address(scanner* s) {
	UChar c = *s->next.address;
	s->next.position++;
	s->next.address++;
	if (s->next.address == s->next.page->block->end) {
		s->next.page = NULL;
		s->next.address = NULL;
	}
	return c;
}

void prepare_next_character(scanner* s) {
	if (s->next.position != s->input.position) {
		return;
	}

	prepare_next_address(s);
	if (s->next.address == NULL) {
		s->next.position++;
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
		s->next.position++;
		s->next_character = MALFORMED;
		return;
	}

	UChar c2 = advance_next_address(s);
	s->next_character = U16_GET_SUPPLEMENTARY(c1, c2);
}

void advance_next_character(scanner* s) {
	s->input = s->next;
}

int64_t advance_input_while(scanner* s, bool (*condition)(UChar32 c, const void* v), const void* context) {
	int64_t from = s->input.position;
	prepare_next_character(s);
	while (s->next_character != EOS &&
			s->next_character != MALFORMED &&
			condition(s->next_character, context)) {
		advance_next_character(s);
		prepare_next_character(s);
	}
	return s->input.position - from;
}

bool advance_input_if(scanner* s, bool (*condition)(UChar32 c, const void* v), const void* context) {
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

bool advance_buffer_page(scanner* s) {
	if (s->buffer.page == s->input.page) {
		return false;
	}
	s->stream->free_block(s->stream, s->buffer.page->block);
	page* next = s->buffer.page->next;
	free(s->buffer.page);
	s->buffer.page = next;
	if (s->buffer.page->block != NULL) {
		s->buffer.address = s->buffer.page->block->start;
	} else {
		s->buffer.address = NULL;
	}
	return true;
}

int64_t take_buffer_to(scanner* s, int64_t p, UChar* b) {
	int64_t size = p - s->buffer.position;
	if (p > s->input.position) {
		p = s->input.position;
	}
	if (size < 0) {
		return size;
	}
	int64_t remaining = size;
	int64_t available = s->buffer.page->block->end - s->buffer.address;
	while (available < remaining) {
		memcpy(b, s->buffer.address, available * sizeof(UChar));
		b += available;
		remaining -= available;
		
		advance_buffer_page(s);
		available = s->buffer.page->block->end - s->buffer.address;
	}
	memcpy(b, s->buffer.address, remaining * sizeof(UChar));
	b += remaining;
	s->buffer.address += remaining;
	s->buffer.position = p;
	return size;
}

int64_t take_buffer_length(scanner* s, int64_t l, UChar* b) {
	return take_buffer_to(s, buffer_position(s) + l, b);
}

int64_t take_buffer(scanner* s, UChar* b) {
	return take_buffer_to(s, input_position(s), b);
}

int64_t discard_buffer_to(scanner* s, int64_t p) {
	int64_t size = p - s->buffer.position;
	if (p > s->input.position) {
		p = s->input.position;
	}
	if (size < 0) {
		return size;
	}
	int64_t remaining = size;
	int64_t available = s->buffer.page->block->end - s->buffer.address;
	while (available < remaining) {
		remaining -= available;
		advance_buffer_page(s);
		available = s->buffer.page->block->end - s->buffer.address;
	}
	return size;
}

int64_t discard_buffer_length(scanner* s, int64_t l) {
	return discard_buffer_to(s, buffer_position(s) + l);
}

int64_t discard_buffer(scanner* s) {
	int64_t size = s->input.position - s->buffer.position;
	while (advance_buffer_page(s));
	s->buffer = s->input;
	return size;
}

void close_scanner(scanner* h) {
	free(h);
}

void close_stream(stream* s) {
	s->close(s);
}

typedef struct ustring_stream {
	UChar* start;
	UChar* end;
} ustring_stream;

block* next_ustring_block(stream* ss) {
	ustring_stream* uss = (ustring_stream*)(ss + 1);

	if (uss->start == NULL) {
		return NULL;
	}

	if (uss->end == NULL) {
		uss->end = uss->start;
		while (*uss->end != u'\0') {
			uss->end++;
		}
	}

	block* b = malloc(sizeof(block));
	b->start = uss->start;
	b->end = uss->end;

	uss->start = NULL;

	return b;
}

void free_ustring_block(stream* s, block* b) {
	free(b);
}

void close_ustring_stream(stream* s) {
	free(s);
}

stream* open_ustring_stream(UChar* s) {
	stream* ss = malloc(sizeof(stream) + sizeof(ustring_stream));
	ustring_stream* uss = (ustring_stream*)(ss + 1);

	ss->next_block = next_ustring_block;
	ss->free_block = free_ustring_block;
	ss->close = close_ustring_stream;

	uss->start = s;
	uss->end = NULL;

	return ss;
}

stream* open_nustring_stream(UChar* s, int64_t l) {
	stream* ss = open_ustring_stream(s);
	ustring_stream* uss = (ustring_stream*)(ss + 1);

	uss->end = s + l;

	return ss;
}
