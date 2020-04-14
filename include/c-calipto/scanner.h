/*
 * Streams
 */

typedef struct block {
	UChar* start;
	UChar* end;
} block;

/**
 * An interface to a stream of data. So long as there is data
 * remaining, calls to next_block will return a non-empty block
 * of data filled with the available content of the stream. When
 * the stream is complete, next_block should return NULL.
 *
 * The caller should always call free_block when they are done
 * with it, so that the stream knows it may clean up any
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
	int64_t position;
	UChar* pointer;
} cursor;

typedef struct scanner {
	cursor input;
	cursor buffer;

	UChar32 next_character;
	cursor next_input;

	stream* stream;
} scanner;

const UChar32 MALFORMED = 0xE000;
const UChar32 PENDING = 0xE001;
const UChar32 EOS = 0xE002;

void open_scanner(stream* b);

void close_scanner(scanner* s);

int64_t input_position(scanner* s);

int64_t buffer_position(scanner* s);

int64_t advance_input_while(scanner* s, void* context, bool (*condition)(UChar32 c, void* context));

bool advance_input_if(scanner* s, void* context, bool (*condition)(UChar32 c, void* context));

int64_t take_buffer_to(scanner* s, int64_t p, UChar* t);

int64_t take_buffer_length(scanner* s, int64_t l, UChar* t);

int64_t take_buffer(scanner* s, UChar* t);

int64_t discard_buffer_to(scanner* s, int64_t p);

int64_t discard_buffer_length(scanner* s, int64_t l);

int64_t discard_buffer(scanner* s);

