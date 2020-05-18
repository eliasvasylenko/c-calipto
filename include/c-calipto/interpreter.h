typedef struct s_binding {
	s_expr name;
	s_expr value;
} s_binding;

/*
 * TODO This has linear lookup time for bindings, this should
 * be rewritten with a proper table lookup
 */
typedef struct s_bindings {
	_Atomic(int32_t)* ref_count;
	struct s_bindings* parent;
	int32_t count;
	s_binding* bindings;
} s_bindings;

typedef struct s_bound_expr {
	s_expr form;
	s_bindings bindings;
} s_bound_expr;

s_bindings s_alloc_bindings(const s_bindings* parent, int32_t count, const s_binding* b);
void s_ref_bindings(s_bindings p);
void s_free_bindings(s_bindings p);
bool s_resolve(s_expr* result, const s_expr name, const s_bindings b);

void eval(const s_expr e, const s_bindings b);

