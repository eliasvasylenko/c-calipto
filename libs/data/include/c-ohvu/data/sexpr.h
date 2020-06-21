void s_init();
void s_close();

typedef enum s_result {
	S_SUCCESS,
	S_ATTEMPT_TO_CALL_NON_FUNCTION,
	S_ARGUMENT_COUNT_MISMATCH
} s_result;

typedef enum s_expr_type {
	ERROR,
	SYMBOL,
	CONS,
	FUNCTION,
	CHARACTER,
	STRING,
	INTEGER,
	BIG_INTEGER
} s_expr_type;

typedef struct s_expr_ref s_expr_ref;

typedef struct s_expr {
	s_expr_type type;
	union {
		UChar32 character;
		int64_t integer;
		s_expr_ref* p;
	};
} s_expr;

typedef struct s_instruction {
	uint32_t size;
	s_expr* values;
} s_instruction;

typedef struct s_table {
	idtrie trie;
} s_table;

typedef struct s_symbol_info {
	s_expr_ref* qualifier;
	UChar name[1];
} s_symbol_info;

typedef struct s_cons_data {
	s_expr car;
	s_expr cdr;
} s_cons_data;

typedef struct s_function_info {
	uint32_t arg_count;
	uint32_t max_result_size;
} s_function_info;

typedef struct s_function_type {
	UChar* name;
	s_expr (*represent)(void* d);
	s_function_info (*inspect)(void* d);
	s_result (*apply)(s_instruction* result, s_expr* args, void* d);
	void (*free) (void* data);
} s_function_type;

typedef struct s_function_data {
	s_function_type* type;
	// variable length data
} s_function_data;

typedef struct s_string_data {
	UChar string[1]; // variable length
} s_string_data;

struct s_expr_ref {
	_Atomic(uint32_t) ref_count;
	union {
		idtrie_node* symbol;
		s_cons_data cons;
		s_function_data function;
		s_string_data string;
	};
};

s_expr s_symbol(s_expr_ref* qualifier, strref name);
s_expr s_cons(s_expr car, s_expr cdr);
s_expr s_character(UChar32 c);
s_expr s_string(strref s);
s_expr s_function(s_function_type* t, uint32_t data_size, void* data);

s_expr s_list(int32_t count, s_expr* e);
s_expr s_list_of(int32_t count, void** e, s_expr (*map)(void* elem));
int32_t s_delist(s_expr l, s_expr** e); 
int32_t s_delist_of(s_expr l, void*** e, void* (*map)(s_expr elem)); 

s_symbol_info* s_inspect(s_expr s);
s_expr s_car(s_expr s);
s_expr s_cdr(s_expr s);
bool s_atom(s_expr e);
bool s_eq(s_expr a, s_expr b);

void s_dump(s_expr s);

s_expr s_alias(s_expr s);
void s_dealias(s_expr s);
s_expr_ref* s_ref(s_expr_ref* r);
void s_free(s_expr_type t, s_expr_ref* r);

