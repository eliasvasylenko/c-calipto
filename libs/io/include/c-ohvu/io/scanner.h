typedef struct ovio_page {
	ovio_block* block;
	struct ovio_page* next;
} ovio_page;

typedef struct ovio_cursor {
	ovio_page* page;
	const UChar* address;
	int64_t position;
} ovio_cursor;

typedef struct ovio_scanner {
	ovio_cursor next;
	ovio_cursor input;
	ovio_cursor buffer;

	UChar32 next_character;

	ovio_stream* stream;
} ovio_scanner;

ovio_scanner* ovio_open_scanner(ovio_stream* s);

void ovio_close_scanner(ovio_scanner* s);

int64_t ovio_input_position(ovio_scanner* s);

int64_t ovio_buffer_position(ovio_scanner* s);

int64_t ovio_advance_input_while(ovio_scanner* s, bool (*condition)(UChar32 c, const void* context), const void* context);

bool ovio_advance_input_if(ovio_scanner* s, bool (*condition)(UChar32 c, const void* context), const void* context);

int64_t ovio_take_buffer_to(ovio_scanner* s, int64_t p, UChar* t);

int64_t ovio_take_buffer_length(ovio_scanner* s, int64_t l, UChar* t);

int64_t ovio_take_buffer(ovio_scanner* s, UChar* t);

int64_t ovio_discard_buffer_to(ovio_scanner* s, int64_t p);

int64_t ovio_discard_buffer_length(ovio_scanner* s, int64_t l);

int64_t ovio_discard_buffer(ovio_scanner* s);

