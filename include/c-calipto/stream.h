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

void close_stream(stream* s);
