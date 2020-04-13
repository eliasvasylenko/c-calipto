typedef struct {
	int64_t position;
	struct cursor_stack* stack;
} cursor_stack;

typedef struct {
	cursor_stack cursor;
	scanner_handle* scanner;
} reader_handle;

reader_handle* open_reader(scanner_handle* s);

void close_reader(reader_handle* r);

int64_t cursor_position(reader_handle* r, int32_t inputDepth);

int32_t cursor_depth(reader_handle* r);

sexpr* read(reader_handle* r);

sexpr* read_symbol(reader_handle* r);

bool read_step_in(reader_handle* r);

sexpr* read_step_out(reader_handle* r);
