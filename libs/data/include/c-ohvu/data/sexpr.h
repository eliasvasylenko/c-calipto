void ovs_init();
void ovs_close();

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
} ovs_table;

typedef struct ovs_symbol_info {
	ovs_expr_ref* qualifier;
	UChar name[1];
} ovs_symbol_info;

typedef struct ovs_cons_data {
	ovs_expr car;
	ovs_expr cdr;
} ovs_cons_data;

typedef struct ovs_function_info {
	uint32_t arg_count;
	uint32_t max_result_size;
} ovs_function_info;

typedef struct ovs_function_type {
	UChar* name;
	ovs_expr (*represent)(void* d);
	ovs_function_info (*inspect)(void* d);
	int32_t (*apply)(ovs_instruction* result, ovs_expr* args, void* d);
	void (*free) (void* data);
} ovs_function_type;

typedef struct ovs_function_data {
	ovs_function_type* type;
	// variable length data
} ovs_function_data;

typedef struct ovs_string_data {
	UChar string[1]; // variable length
} ovs_string_data;

struct ovs_expr_ref {
	_Atomic(uint32_t) ref_count;
	union {
		bdtrie_node* symbol;
		ovs_cons_data cons;
		ovs_function_data function;
		ovs_string_data string;
	};
};

ovs_expr ovs_symbol(ovs_expr_ref* qualifier, ovio_strref name);
ovs_expr ovs_cons(ovs_expr car, ovs_expr cdr);
ovs_expr ovs_character(UChar32 c);
ovs_expr ovs_string(ovio_strref s);
ovs_expr ovs_function(ovs_function_type* t, uint32_t data_size, void* data);

ovs_expr ovs_list(int32_t count, ovs_expr* e);
ovs_expr ovs_list_of(int32_t count, void** e, ovs_expr (*map)(void* elem));
int32_t ovs_delist(ovs_expr l, ovs_expr** e); 
int32_t ovs_delist_of(ovs_expr l, void*** e, void* (*map)(ovs_expr elem)); 

ovs_symbol_info* ovs_inspect(ovs_expr s);
ovs_expr ovs_car(ovs_expr s);
ovs_expr ovs_cdr(ovs_expr s);
bool ovs_atom(ovs_expr e);
bool ovs_eq(ovs_expr a, ovs_expr b);

void ovs_dump(ovs_expr s);

ovs_expr ovs_alias(ovs_expr s);
void ovs_dealias(ovs_expr s);
ovs_expr_ref* ovs_ref(ovs_expr_ref* r);
void ovs_free(ovs_expr_type t, ovs_expr_ref* r);

