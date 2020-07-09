typedef enum ovs_expr_type {
	OVS_SYMBOL,
	OVS_CONS,
	OVS_FUNCTION,
	OVS_CHARACTER,
	OVS_STRING,
	OVS_INTEGER,
	OVS_BIG_INTEGER
} ovs_expr_type;

typedef struct ovs_expr_ref ovs_expr_ref;

typedef struct ovs_expr {
	ovs_expr_type type;
	union {
		UChar32 character;
		int64_t integer;
		ovs_expr_ref* p;
	};
} ovs_expr;

typedef struct ovs_instruction {
	uint32_t size;
	ovs_expr* values;
} ovs_instruction;

typedef struct ovs_table {
	bdtrie trie;
	ovs_expr_ref* qualifier;
} ovs_table;

typedef struct ovs_context {
	ovs_table table;
	ovs_expr_ref* data;
	ovs_expr_ref* nil;
	ovs_expr_ref* quote;
} ovs_context;

typedef struct ovs_symbol_data {
	bdtrie_node* node;
	ovs_table* table;
} ovs_symbol_data;

typedef struct ovs_cons_data {
	ovs_table* table;
	ovs_expr car;
	ovs_expr cdr;
} ovs_cons_data;

typedef struct ovs_function_info {
	uint32_t arg_count;
	uint32_t max_result_size;
} ovs_function_info;

typedef struct ovs_function_type {
	ovs_expr (*represent)(ovs_context* c, void* d);
	ovs_function_info (*inspect)(void* d);
	int32_t (*apply)(ovs_instruction* result, ovs_expr* args, void* d);
	void (*free) (void* data);
} ovs_function_type;

typedef struct ovs_function_data {
	ovs_function_type* type;
	ovs_context* context;
	// variable length data
} ovs_function_data;

typedef struct ovs_string_data {
	UChar string[1]; // variable length
} ovs_string_data;

struct ovs_expr_ref {
	_Atomic(uint32_t) ref_count;
	union {
		ovs_symbol_data symbol;
		ovs_cons_data cons;
		ovs_function_data function;
		ovs_string_data string;
	};
};

ovs_context ovs_init();
void ovs_close(ovs_context c);

ovs_expr ovs_symbol(ovs_table* t, ovio_strref name);
ovs_expr ovs_cons(ovs_table* t, ovs_expr car, ovs_expr cdr);
ovs_expr ovs_character(UChar32 c);
ovs_expr ovs_string(ovio_strref s);
ovs_expr ovs_function(ovs_context* c, ovs_function_type* t, uint32_t data_size, void* data);

ovs_expr ovs_list(ovs_table* t, int32_t count, ovs_expr* e);
ovs_expr ovs_list_of(ovs_table* t, int32_t count, void** e, ovs_expr (*map)(void* elem));
int32_t ovs_delist(ovs_expr l, ovs_expr** e); 
int32_t ovs_delist_of(ovs_expr l, void*** e, void* (*map)(ovs_expr elem)); 

bool ovs_is_atom(ovs_table* t, ovs_expr e);
bool ovs_is_qualified(ovs_expr e);
bool ovs_is_symbol(ovs_expr e);
bool ovs_is_eq(ovs_expr a, ovs_expr b);

ovs_expr ovs_qualifier(ovs_context* c, ovs_expr e);
UChar* ovs_name(ovs_expr e);
ovs_expr ovs_car(ovs_expr e);
ovs_expr ovs_cdr(ovs_expr e);

void ovs_dump(ovs_expr s);

ovs_expr ovs_alias(ovs_expr s);
void ovs_dealias(ovs_expr s);
ovs_expr_ref* ovs_ref(ovs_expr_ref* r);
void ovs_free(ovs_expr_type t, ovs_expr_ref* r);

