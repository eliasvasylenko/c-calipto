typedef struct cursor_stack {
	int64_t position;
	struct cursor_stack* stack;
} cursor_stack;

typedef struct reader_handle {
	cursor_stack cursor;
	scanner_handle* scanner;
} reader_handle;

reader_handle* open_reader(scanner_handle* h);

void close_reader(reader_handle* h);

int64_t cursor_position(int32_t inputDepth);

int32_t cursor_depth(void);

void* read(void);

void* read_symbol(void);

bool read_step_in(void);

int32_t read_step_out(void);
