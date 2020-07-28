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
		ovs_expr_ref const* p;
	};
} ovs_expr;

typedef struct ovs_instruction {
	uint32_t size;
	ovs_expr* values;
} ovs_instruction;

typedef enum ovs_root_table {
	OVS_UNQUALIFIED,
	OVS_DATA,
	OVS_DATA_NIL,
	OVS_DATA_QUOTE,
	OVS_DATA_LAMBDA,
	OVS_SYSTEM,
	OVS_SYSTEM_BUILTIN,
	OVS_TEXT,
	OVS_TEXT_STRING,
	OVS_TEXT_CHARACTER
} ovs_root_table;
#define OVS_ROOT_TABLE_COUNT (OVS_TEXT_CHARACTER + 1)

typedef struct ovs_table {
	bdtrie trie;
	ovs_expr_ref* qualifier;
} ovs_table;

typedef struct ovs_context {
	ovs_table root_tables[OVS_ROOT_TABLE_COUNT];
} ovs_context;

typedef struct ovs_symbol_data {
	bdtrie_node* node;
	union {
		ovs_table* table;
		uint32_t offset;
	};
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

struct ovs_function_data;

typedef struct ovs_function_type {
	UChar* name;
	ovs_expr (*represent)(const struct ovs_function_data* d);
	ovs_function_info (*inspect)(const void* d);
	int32_t (*apply)(ovs_instruction* result,  ovs_expr* args, const struct ovs_function_data* d);
	void (*free) (const void* data);
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

/*
 * Core symbols
 */

typedef struct ovs_root_symbol_data {
	int32_t nameSize;
	UChar* name;
	ovs_root_table qualifier;
	ovs_expr_ref data;
	ovs_expr expr;
} ovs_root_symbol_data;

ovs_context* ovs_init();
void ovs_close(ovs_context* c);

ovs_table* ovs_table_for(ovs_context* c, const ovs_expr_ref* r);
ovs_table* ovs_table_of(ovs_context* c, const ovs_expr e);
ovs_context* ovs_context_of(ovs_table* t);

ovs_expr ovs_symbol(ovs_table* t, uint32_t l, UChar* name);
ovs_root_symbol_data* ovs_root_symbol(ovs_root_table t);
ovs_expr ovs_cons(ovs_table* t, ovs_expr car, ovs_expr cdr);
ovs_expr ovs_character(UChar32 c);
ovs_expr ovs_string(uint32_t l, UChar* s);
ovs_expr ovs_cstring(UConverter* c, char* s);
ovs_expr ovs_function(ovs_context* c, ovs_function_type* t, uint32_t extra_data_size, void** extra_data);

ovs_expr ovs_list(ovs_table* t, int32_t count, ovs_expr* e);
ovs_expr ovs_list_of(ovs_table* t, int32_t count, void** e, ovs_expr (*map)(const void* elem));
int32_t ovs_delist(ovs_table* t, ovs_expr l, ovs_expr** e); 
int32_t ovs_delist_of(ovs_table* t, ovs_expr l, void*** e, void* (*map)(ovs_expr elem)); 

bool ovs_is_atom(ovs_table* t, ovs_expr e);
bool ovs_is_qualified(ovs_expr e);
bool ovs_is_symbol(ovs_expr e);
bool ovs_is_eq(ovs_expr a, ovs_expr b);

ovs_expr ovs_qualifier(ovs_expr e);
UChar* ovs_name(ovs_expr e);
ovs_expr ovs_car(ovs_expr e);
ovs_expr ovs_cdr(ovs_expr e);

void ovs_dump_expr(const ovs_expr s);
void ovs_dump_context(const ovs_context* c);

ovs_expr ovs_alias(ovs_expr s);
void ovs_dealias(ovs_expr s);
const ovs_expr_ref* ovs_ref(const ovs_expr_ref* r);
void ovs_free(ovs_expr_type t, const ovs_expr_ref* r);

