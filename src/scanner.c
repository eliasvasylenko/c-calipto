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
#include <unicode/ustdio.h>

#include "c-calipto/stream.h"
#include "c-calipto/scanner.h"

const UChar32 MALFORMED = 0xE000;
const UChar32 EOS = 0xE001;

scanner* open_scanner(stream* s) {
	scanner* sc = malloc(sizeof(scanner));

	sc->stream = s;
	
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
			s->input = s->next;
			s->buffer = s->next;
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
	s->buffer.address += remaining;
	s->buffer.position = p;
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

void close_scanner(scanner* s) {
	discard_buffer(s);
	if (s->input.page != NULL) {
		if (s->input.page->block != NULL) {
			s->stream->free_block(s->stream, s->input.page->block);
		}
		free(s->input.page);
	}
	free(s);
}

