typedef struct page {
	UChar* start;
	UChar* end;
	page* next;
} page;

typedef struct buffer {
	page* (*fill_page)(buffer* b);
	void (*clear_page)(buffer* b, page* p);
	void (*close)(struct buffer* b);
} buffer;

typedef struct {
	page* input_page;
	int64_t input_position;
	UChar* input_pointer;

	page* buffer_page;
	int64_t buffer_position;
	UChar* buffer_pointer;

	buffer* buffer;
} scanner;

void open_scanner(buffer* b);

void close_scanner(scanner* s);

int64_t input_position(scanner* s);

int64_t buffer_position(scanner* s);

int64_t advance_input_while(scanner* s, bool (*condition)(char32_t));

bool advance_input_if(scanner* s, bool (*condition)(char32_t));

bool advance_input_if_equal(scanner* s, char32_t c);

int64_t take_buffer_to(scanner* s, int64_t p, char32_t* s);

int64_t take_buffer_length(scanner* s, int64_t l, char32_t* s);

int64_t take_buffer(scanner* s, char32_t* s);

int64_t discard_buffer_to(scanner* s, int64_t p);

int64_t discard_buffer_length(scanner* s, int64_t l);

int64_t discard_buffer(scanner* s);

buffer* open_file_buffer(FILE* f);
buffer* open_string_buffer(char* s);
buffer* open_nstring_buffer(char* s, int64_t l);
buffer* open_ustring_buffer(UChar* s);
buffer* open_nustring_buffer(UChar* s, int64_t l);
buffer* open_stdin_buffer();

