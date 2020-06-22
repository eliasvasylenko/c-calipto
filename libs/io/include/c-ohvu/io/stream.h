typedef struct ovio_block {
	const UChar* start;
	const UChar* end;
} ovio_block;

/**
 * An interface to a stream of data. So long as there is data
 * remaining in the stream, calls to next_block should return a
 * non-empty block filled with the next available bytes.
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
typedef struct ovio_stream {
	ovio_block* (*next_block)(struct ovio_stream* b);
	void (*free_block)(struct ovio_stream* b, ovio_block* p);
	void (*close)(struct ovio_stream* b);
} ovio_stream;

ovio_stream* ovio_open_file_stream(UFILE* f);
ovio_stream* ovio_open_string_stream(const char* s);
ovio_stream* ovio_open_nstring_stream(const char* s, int64_t l);
ovio_stream* ovio_open_ustring_stream(const UChar* s);
ovio_stream* ovio_open_nustring_stream(const UChar* s, int64_t l);
ovio_stream* ovio_open_stdin_stream();

void ovio_close_stream(ovio_stream* s);
