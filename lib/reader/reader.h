typedef struct cursor_stack {
	int64_t position;
	struct cursor_stack* stack;
} cursor_stack;

typedef struct reader {
	cursor_stack cursor;
	scanner* scanner;
	s_expr data_quote;
	s_expr data_nil;
} reader;

reader* open_reader(scanner* s);

void close_reader(reader* r);

int64_t cursor_position(reader* r, int32_t inputDepth);

int32_t cursor_depth(reader* r);

bool read(reader* r, s_expr* e);

bool read_symbol(reader* r, s_expr* e);

bool read_step_in(reader* r);

bool read_step_out(reader* r, s_expr* e);
