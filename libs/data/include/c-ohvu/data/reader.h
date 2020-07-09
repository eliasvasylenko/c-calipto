typedef enum ovda_result {
	OVDA_SUCCESS,
	OVDA_UNEXPECTED_TYPE,
	OVDA_INVALID
} ovda_result;

typedef struct ovda_cursor_stack {
	int64_t position;
	struct ovda_cursor_stack* stack;
} ovda_cursor_stack;

typedef struct ovda_reader {
	ovda_cursor_stack cursor;
	ovio_scanner* scanner;
	ovs_context* context;
} ovda_reader;

ovda_reader* ovda_open_reader(ovio_scanner* s, ovs_context* c);

void ovda_close_reader(ovda_reader* r);

int64_t ovda_cursor_position(ovda_reader* r, int32_t inputDepth);

int32_t ovda_cursor_depth(ovda_reader* r);

ovda_result ovda_read(ovda_reader* r, ovs_expr* e);

ovda_result ovda_read_symbol(ovda_reader* r, ovs_expr* e);

ovda_result ovda_read_step_in(ovda_reader* r);

ovda_result ovda_read_step_out(ovda_reader* r, ovs_expr* e);
