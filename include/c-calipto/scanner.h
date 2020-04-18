typedef struct page {
	block* block;
	struct page* next;
} page;

typedef struct cursor {
	page* page;
	UChar* address;
	int64_t position;
} cursor;

typedef struct scanner {
	cursor next;
	cursor input;
	cursor buffer;

	UChar32 next_character;

	stream* stream;
} scanner;

scanner* open_scanner(stream* s);

void close_scanner(scanner* s);

int64_t input_position(scanner* s);

int64_t buffer_position(scanner* s);

int64_t advance_input_while(scanner* s, bool (*condition)(UChar32 c, const void* context), const void* context);

bool advance_input_if(scanner* s, bool (*condition)(UChar32 c, const void* context), const void* context);

int64_t take_buffer_to(scanner* s, int64_t p, UChar* t);

int64_t take_buffer_length(scanner* s, int64_t l, UChar* t);

int64_t take_buffer(scanner* s, UChar* t);

int64_t discard_buffer_to(scanner* s, int64_t p);

int64_t discard_buffer_length(scanner* s, int64_t l);

int64_t discard_buffer(scanner* s);

