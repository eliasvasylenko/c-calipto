#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>

#include <sexpr.h>
#include <scanner.h>

scanner_handle* scanner_init(scanner_type type, int32_t payload_size) {
	scanner_handle* h = malloc(sizeof(scanner_handle) + payload_size);
	h->type = type;

	return h;
}

scanner_handle* scan_file(FILE* f) {
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

scanner_handle* scan_string(char* s) {
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
			if (*sh->input == '\0') {
				break;
			}
			char32_t c;
			int s = mbrtoc32(&c, sh->input, MB_CUR_MAX, NULL);
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

int64_t take_buffer(scanner_handle* h, char32_t* s) {
	switch (h->type) {
	case FILE_SCANNER:
		break;
	case STRING_SCANNER:;
		string_handle* sh = (string_handle*)(h + 1);

		int64_t size = sh->input_pos - sh->buffer_pos;
		sh->buffer_pos = sh->input_pos;
		for (int j = 0; j < size; j++) {
			sh->input += mbrtoc32(s + j, sh->input, MB_CUR_MAX, NULL);
		}
		*(s + size) = U'\0';
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
