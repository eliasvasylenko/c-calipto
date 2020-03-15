typedef enum scanner_type {
	FILE_SCANNER,
	STRING_SCANNER,
	STD_IO_SCANNER,
	SEXPR_SCANNER
} scanner_type;

typedef struct scanner_handle {
        scanner_type type;
} scanner_handle;

scanner_handle* scan_file(FILE* f);

scanner_handle* scan_string(char* s);

scanner_handle* scan_stdin();

scanner_handle* scan_sexpr(sexpr* e);

void close_scanner(scanner_handle* h);

int64_t input_position(scanner_handle* h);

int64_t buffer_position(scanner_handle* h);

int64_t advance_input_while(scanner_handle* h, bool (*condition)(char32_t));

bool advance_input_if(scanner_handle* h, bool (*condition)(char32_t));

int64_t take_buffer_to(scanner_handle* h, int64_t position, char32_t* s);

int64_t take_buffer(scanner_handle* h, char32_t* s);

void discard_buffer_to(scanner_handle* h, int64_t position);

void discard_buffer(scanner_handle* h);
