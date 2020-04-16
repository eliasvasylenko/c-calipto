/*
 * Streams
 */

typedef struct block {
	UChar* start;
	UChar* end;
} block;

/**
 * An interface to a stream of data. So long as there is data
 * remaining in the stream, calls to next_block should return a
 * non-empty block filled with the next available bytes of the.
 *
 * When the stream is complete, next_block should return NULL.
 *
 * For every block obtained from next_block, the caller should
 * call free_block when they are done with it and before
 * calling close so that the stream knows it may clean up any
 * resources associated with the block.
 *
 * The caller should always call close when they are done with a
 * stream, this may occur before they have reached the end of
 * the stream.
 */
typedef struct stream {
	block* (*next_block)(struct stream* b);
	void (*free_block)(struct stream* b, block* p);
	void (*close)(struct stream* b);
} stream;

stream* open_file_stream(FILE* f);
stream* open_string_stream(char* s);
stream* open_nstring_stream(char* s, int64_t l);
stream* open_ustring_stream(UChar* s);
stream* open_nustring_stream(UChar* s, int64_t l);
stream* open_stdin_stream();

/*
 * Scanners
 */

typedef struct page {
	block* block;
	struct page* next;
} page;

typedef struct cursor {
	page* page;
	UChar* address;
	int64_t size;
	int64_t position;
} cursor;

typedef struct scanner {
	cursor next;
	cursor input;
	cursor buffer;

	UChar32 next_character;

	stream* stream;
} scanner;

const UChar32 MALFORMED = 0xE000;
const UChar32 EOS = 0xE001;

scanner* open_scanner(stream* s);

void close_scanner(scanner* s);

int64_t input_position(scanner* s);

int64_t buffer_position(scanner* s);

int64_t advance_input_while(scanner* s, bool (*condition)(UChar32 c, void* context), void* context);

bool advance_input_if(scanner* s, bool (*condition)(UChar32 c, void* context), void* context);

int64_t take_buffer_to(scanner* s, int64_t p, UChar* t);

int64_t take_buffer_length(scanner* s, int64_t l, UChar* t);

int64_t take_buffer(scanner* s, UChar* t);

int64_t discard_buffer_to(scanner* s, int64_t p);

int64_t discard_buffer_length(scanner* s, int64_t l);

int64_t discard_buffer(scanner* s);

