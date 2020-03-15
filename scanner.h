typedef enum scanner_type {
	FILE_SCANNER,
	STRING_SCANNER,
	STD_IO_SCANNER,
	SEXPR_SCANNER
} scanner_type;

typedef struct scanner_handle {
        scanner_type type;
} scanner_handle;

scanner_handle* open_file_scanner(FILE* f);

scanner_handle* open_string_scanner(char* s);

scanner_handle* open_stdin_scanner();

scanner_handle* open_sexpr_scanner(sexpr* e);

void close_scanner(scanner_handle* h);

int64_t input_position(scanner_handle* h);

int64_t buffer_position(scanner_handle* h);

int64_t advance_input_while(scanner_handle* h, bool (*condition)(char32_t));

bool advance_input_if(scanner_handle* h, bool (*condition)(char32_t));

int64_t take_buffer_to(scanner_handle* h, int64_t position, char32_t* s);

int64_t take_buffer_length(scanner_handle* h, int64_t l, char32_t* s);

int64_t take_buffer(scanner_handle* h, char32_t* s);

void discard_buffer_to(scanner_handle* h, int64_t position);

void discard_buffer_length(scanner_handle* h, int64_t l);

void discard_buffer(scanner_handle* h);
