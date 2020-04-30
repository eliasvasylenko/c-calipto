typedef struct cursor_stack {
	int64_t position;
	struct cursor_stack* stack;
} cursor_stack;

typedef struct reader {
	cursor_stack cursor;
	scanner* scanner;
} reader;

reader* open_reader(scanner* s);

void close_reader(reader* r);

int64_t cursor_position(reader* r, int32_t inputDepth);

int32_t cursor_depth(reader* r);

s_expr read(reader* r);

s_expr read_symbol(reader* r);

bool read_step_in(reader* r);

s_expr read_step_out(reader* r);
