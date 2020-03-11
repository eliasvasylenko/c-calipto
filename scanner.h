typedef enum scanner_type {
        FILE_SCANNER,
        STRING_SCANNER,
        STD_IO_SCANNER
} scanner_type;

typedef struct scanner_handle {
        scanner_type type;
} scanner_handle;

int64_t input_position(scanner_handle* handle);

int64_t buffer_position(scanner_handle* handle);

int64_t advance_input_while(scanner_handle* handle, bool (*condition)(char32_t));

bool advance_input_if(scanner_handle* handle, bool (*condition)(char32_t));

int64_t take_buffer_to(scanner_handle* handle, int64_t position, int64_t buffer_size, char32_t* string);

void discard_buffer_to(scanner_handle* handle, int64_t position);

char32_t* utf8to32(char* text);
