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
