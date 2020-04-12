#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>
#include <limits.h>

#include <c-calipto/sexpr.h>
#include <c-calipto/scanner.h>

scanner_handle* scanner_init(scanner_type type, int32_t payload_size) {
	scanner_handle* h = malloc(sizeof(scanner_handle) + payload_size);
	h->type = type;

	return h;
}

scanner_handle* open_file_scanner(FILE* f) {
	scanner_handle* h = scanner_init(FILE_SCANNER, sizeof(FILE*));
	FILE** p = (FILE**)(h + 1);
	*p = f;

	return h;
}

typedef struct string_handle {
	char* input;
	int64_t input_pos;
	char* buffer;
	int64_t buffer_pos;
} string_handle;

scanner_handle* open_string_scanner(char* s) {
	scanner_handle* h = scanner_init(STRING_SCANNER, sizeof(string_handle));
	string_handle* sh = (string_handle*)(h + 1);
	sh->input = s;
	sh->input_pos = 0;
	sh->buffer = s;
	sh->buffer_pos = 0;

	return h;
}

int64_t input_position(scanner_handle* h) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:;
		string_handle* sh = (string_handle*)(h + 1);
		return sh->input_pos;
	case STD_IO_SCANNER:
		break;
	case SEXPR_SCANNER:
		break;
	}
	return -1;
}

int64_t buffer_position(scanner_handle* h) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:;
		string_handle* sh = (string_handle*)(h + 1);
		return sh->buffer_pos;
	case STD_IO_SCANNER:
		break;
	case SEXPR_SCANNER:
		break;
	}
	return -1;
}

int64_t advance_input_while(scanner_handle* h, bool (*condition)(char32_t)) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:;
		string_handle* sh = (string_handle*)(h + 1);
		int64_t from = sh->input_pos;
		while (1) {
			if (*sh->input == U'\0') {
				break;
			}
			char32_t c;
			int s = mbrtoc32(&c, sh->input, MB_LEN_MAX, NULL);
			if (condition(c)) {
				sh->input_pos++;
				sh->input += s;
			} else {
				break;
			}
		}
		return sh->input_pos - from;
	case STD_IO_SCANNER:
		break;
	case SEXPR_SCANNER:
		break;
	}
	return -1;
}

bool advance_input_if(scanner_handle* h, bool (*condition)(char32_t)) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:;
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
	case STD_IO_SCANNER:
		break;
	case SEXPR_SCANNER:
		break;
	}
	return -1;
}

bool advance_input_if_equal(scanner_handle* h, char32_t c) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:;
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
	case STD_IO_SCANNER:
		break;
	case SEXPR_SCANNER:
		break;
	}
	return -1;
}

int64_t take_buffer_to(scanner_handle* h, int64_t p, char32_t* s) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:;
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
	case STD_IO_SCANNER:
		break;
	case SEXPR_SCANNER:
		break;
	}
	return -1;
}

int64_t take_buffer_length(scanner_handle* h, int64_t l, char32_t* s) {
	return take_buffer_to(h, buffer_position(h) + l, s);
}

int64_t take_buffer(scanner_handle* h, char32_t* s) {
	return take_buffer_to(h, input_position(h), s);
}

int64_t discard_buffer_to(scanner_handle* h, int64_t p) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:;
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
	case STD_IO_SCANNER:
		break;
	case SEXPR_SCANNER:
		break;
	}
	return -1;
}

int64_t discard_buffer_length(scanner_handle* h, int64_t l) {
	return discard_buffer_to(h, buffer_position(h) + l);
}

int64_t discard_buffer(scanner_handle* h) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:;
		string_handle* sh = (string_handle*)(h + 1);

		int64_t size = sh->input_pos - sh->buffer_pos;
		sh->buffer = sh->input;
		sh->buffer_pos = sh->input_pos;
		return size;
	case STD_IO_SCANNER:
		break;
	case SEXPR_SCANNER:
		break;
	}
	return -1;
}

void close_scanner(scanner_handle* h) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:
		break;
	case STD_IO_SCANNER:
		break;
	case SEXPR_SCANNER:
		break;
	}
	free(h);
}
