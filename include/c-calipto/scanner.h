typedef struct page {
	UChar* start;
	UChar* end;
	struct page* next;
} page;

typedef struct buffer {
	page* (*fill_page)(struct buffer* b);
	void (*clear_page)(struct buffer* b, page* p);
	void (*close)(struct buffer* b);
} buffer;

typedef struct {
	page* page;
	int64_t position;
	UChar* pointer;
} cursor;

typedef struct {
	cursor input_cursor;
	cursor buffer_cursor
	buffer* buffer;
} scanner;

void open_scanner(buffer* b);

void close_scanner(scanner* s);

int64_t input_position(scanner* s);

int64_t buffer_position(scanner* s);

int64_t advance_input_while(scanner* s, bool (*condition)(UChar32));

bool advance_input_if(scanner* s, bool (*condition)(UChar32));

bool advance_input_if_equal(scanner* s, UChar32 c);

int64_t take_buffer_to(scanner* s, int64_t p, UChar* t);

int64_t take_buffer_length(scanner* s, int64_t l, UChar* t);

int64_t take_buffer(scanner* s, UChar* t);

int64_t discard_buffer_to(scanner* s, int64_t p);

int64_t discard_buffer_length(scanner* s, int64_t l);

int64_t discard_buffer(scanner* s);

buffer* open_file_buffer(FILE* f);
buffer* open_string_buffer(char* s);
buffer* open_nstring_buffer(char* s, int64_t l);
buffer* open_ustring_buffer(UChar* s);
buffer* open_nustring_buffer(UChar* s, int64_t l);
buffer* open_stdin_buffer();

